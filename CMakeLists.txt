cmake_minimum_required(VERSION 3.15)
project(VideoDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Incluir cabeçalhos locais
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Encontrar cURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Fontes do projeto
set(SOURCES
    src/main.cpp
    src/IniParser.cpp
    src/Downloader.cpp
    src/VideoPlayer.cpp
    src/MainWindow.cpp
)

set(HEADERS
    include/IniParser.h
    include/Downloader.h
    include/VideoPlayer.h
    include/MainWindow.h
)

# Executável do projeto
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Flags para MinGW (estático para libgcc e libstdc++)
if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

# Bibliotecas de link
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CURL_LIBRARIES}
    strmiids
    ole32
    oleaut32
    uuid
    comctl32
    shell32
    gdi32
    user32
    ws2_32
    crypt32
    wldap32
)

# Garantir UTF-8 e definições do Windows
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

# Criar pasta de downloads se não existir
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/downloads)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/downloads)
