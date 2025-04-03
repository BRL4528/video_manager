#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <map>  // Include the map header

namespace fs = std::filesystem;
const std::string VIDEO_DIR = "/home/jet/Videos/";
const std::string FFMPEG_PRIMARY = "ffmpeg -y -re -i ";
const std::string FFMPEG_SECONDARY = " -c:v libx264 -preset ultrafast -tune zerolatency -b:v 1M -maxrate 1M -bufsize 2M -g 30 -f flv rtmp://node.pigtek.com.br/live/";
const std::string FFMPEG_SECRET = "?secret=91fb9742b27e49ca893475f1ed245edc";

bool isFileComplete(const fs::path& file) {
    static std::map<std::string, uintmax_t> fileSizes;

    uintmax_t newSize = fs::file_size(file);
    if (fileSizes[file.string()] == newSize) {
        return true; // O tamanho do arquivo não mudou, está completo.
    }
    
    fileSizes[file.string()] = newSize;
    return false; // Ainda está sendo gravado.
}

void processVideos() {
    while (true) {
        for (const auto& entry : fs::directory_iterator(VIDEO_DIR)) {
            if (entry.path().extension() == ".avi") {
                std::string videoFile = entry.path().string();

                if (isFileComplete(entry.path())) {
                    std::string ffmpegCmd = FFMPEG_PRIMARY + videoFile + FFMPEG_SECONDARY + entry.path().stem().string() + FFMPEG_SECRET;
                    
                    std::cout << "📤 Enviando: " << videoFile << std::endl;
                    int result = std::system(ffmpegCmd.c_str());

                    if (result == 0) {
                        std::cout << "✅ Upload concluído. Excluindo: " << videoFile << std::endl;
                        fs::remove(videoFile);
                    } else {
                        std::cerr << "❌ Falha ao enviar: " << videoFile << std::endl;
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(10)); // Espera 10s antes de checar novamente.
    }
}

int main() {
    std::cout << "🎥 Monitor de envio de vídeos iniciado..." << std::endl;
    processVideos();
    return 0;
}
