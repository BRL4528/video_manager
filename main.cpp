#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdlib>
#include <map>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ixwebsocket/IXWebSocket.h>
#include <curl/curl.h>


namespace fs = std::filesystem;

// Constantes
const std::string VIDEO_DIR = "/home/jet/Videos/";
const std::string FFMPEG_PRIMARY = "ffmpeg -y -re -i ";
const std::string FFMPEG_SECONDARY = " -c:v libx264 -preset ultrafast -tune zerolatency -b:v 1M -maxrate 1M -bufsize 2M -g 30 -f flv rtmp://node.pigtek.com.br/live/";
const std::string FFMPEG_SECRET = "?secret=91fb9742b27e49ca893475f1ed245edc";

// Controle de Pause/Resume
std::atomic<bool> paused(false);

// Controle do processo ffmpeg atual
std::atomic<pid_t> ffmpeg_pid(-1);

// Fila e sincronização
std::queue<std::string> uploadQueue;
std::mutex queueMutex;
std::condition_variable queueCV;

void notifyValidation() {
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://node.pigtek.com.br/scores/validate");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "⚠️ Erro ao chamar rota de validação: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "📡 Validação notificada com sucesso!" << std::endl;
        }
        curl_easy_cleanup(curl);
    }
}


// Função para checar se arquivo está completo
bool isFileComplete(const fs::path& file) {
    static std::map<std::string, uintmax_t> fileSizes;

    uintmax_t newSize = fs::file_size(file);
    if (fileSizes[file.string()] == newSize) {
        return true;
    }

    fileSizes[file.string()] = newSize;
    return false;
}

// Thread para escanear e adicionar vídeos na fila
void scanVideos() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!paused) {
                for (const auto& entry : fs::directory_iterator(VIDEO_DIR)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".avi") {
                        const auto& path = entry.path();
                        if (isFileComplete(path)) {
                            std::string videoFile = path.string();
                            // Evita inserir duplicados na fila
                            bool alreadyInQueue = false;
                            std::queue<std::string> tempQueue = uploadQueue;
                            while (!tempQueue.empty()) {
                                if (tempQueue.front() == videoFile) {
                                    alreadyInQueue = true;
                                    break;
                                }
                                tempQueue.pop();
                            }
                            if (!alreadyInQueue) {
                                uploadQueue.push(videoFile);
                                queueCV.notify_one();
                                std::cout << "🆕 Vídeo adicionado à fila: " << videoFile << std::endl;
                            }
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// Função para enviar um vídeo
bool uploadVideo(const std::string& filePath) {
    std::string command = FFMPEG_PRIMARY + "\"" + filePath + "\"" + FFMPEG_SECONDARY + fs::path(filePath).stem().string() + FFMPEG_SECRET;
    std::cout << "📤 Iniciando envio de: " << filePath << std::endl;

    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127); // execl falhou
    } else if (pid < 0) {
        std::cerr << "❌ Falha ao criar processo filho para ffmpeg" << std::endl;
        return false;
    } else {
        // Processo pai
        ffmpeg_pid = pid;
        int status;
        waitpid(pid, &status, 0);
        ffmpeg_pid = -1;

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::cout << "✅ Upload concluído, excluindo: " << filePath << std::endl;
        try {
            fs::remove(filePath);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "⚠️ Erro ao excluir arquivo: " << e.what() << std::endl;
        }
        } else {
            std::cerr << "❌ Falha no envio: " << filePath << std::endl;
        }

        notifyValidation(); // chamada à API, independentemente do resultado
        return true;
        }
}

// Thread que processa a fila de uploads
void processUploads() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCV.wait(lock, [] { return !uploadQueue.empty(); });

        while (!uploadQueue.empty()) {
            if (paused) {
                std::cout << "⏸️ Upload pausado. Aguardando retomada..." << std::endl;
                queueCV.wait(lock, [] { return !paused.load(); });
            }

            std::string filePath = uploadQueue.front();
            uploadQueue.pop();
            lock.unlock();

            uploadVideo(filePath);

            lock.lock();
        }
    }
}

// Thread de comunicação WebSocket
void websocketListener(const std::string& wsUrl) {
    ix::WebSocket webSocket;
    webSocket.setUrl(wsUrl);

    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            if (msg->str == "station_started") {
                if (ffmpeg_pid > 0) {
                    kill(ffmpeg_pid, SIGTERM);
                    waitpid(ffmpeg_pid, nullptr, 0);
                    ffmpeg_pid = -1;
                }
                paused = true;
                std::cout << "🚨 Upload pausado e ffmpeg terminado (station_started recebido)" << std::endl;
            }
            else if (msg->str == "program_finalized") {
                paused = false;
                queueCV.notify_all(); // Acorda upload thread se estiver esperando
                std::cout << "✅ Upload retomado (program_finalized recebido)" << std::endl;
            }
        }
    });

    webSocket.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }
}

int main() {
    std::cout << "🎥 Monitor de envio de vídeos iniciado..." << std::endl;
    curl_global_init(CURL_GLOBAL_DEFAULT);


    const std::string wsUrl = "ws://localhost:3333";

    std::thread scannerThread(scanVideos);
    std::thread uploaderThread(processUploads);
    std::thread websocketThread(websocketListener, wsUrl);

    scannerThread.join();
    uploaderThread.join();
    websocketThread.join();
    curl_global_cleanup();


    return 0;
}
