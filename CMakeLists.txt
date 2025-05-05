cmake_minimum_required(VERSION 3.10)

# Nome do projeto
project(uploader-monitor)

# Versão do C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Buscar pacotes necessários
find_package(ixwebsocket REQUIRED)

find_package(ZLIB REQUIRED)

# Adicionar o executável
add_executable(uploader-monitor main.cpp)

target_link_libraries(uploader-monitor PRIVATE ZLIB::ZLIB)



# Incluir o diretório de headers do ixwebsocket
target_include_directories(uploader-monitor PRIVATE /usr/include/ixwebsocket)

# Linkar a biblioteca ixwebsocket
target_link_libraries(uploader-monitor
    PUBLIC
    ixwebsocket
    z
)

# Outras flags opcionais para garantir warnings
target_compile_options(uploader-monitor PRIVATE -Wall -Wextra -Wpedantic)
