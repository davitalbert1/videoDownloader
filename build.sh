# Script de automação de build para o VideoDownloader (MinGW-w64 + CMake)

set -e

BUILD_DIR="build"
DLL_DIR="dll"

# Diretório raiz do projeto (onde o VideoDownloader.exe será gerado, fora do build)
PROJECT_DIR="$(pwd)"

# Função para exibir ajuda
show_help() {
    echo "Uso: ./build.sh [OPCOES]"
    echo ""
    echo "Opções:"
    echo "  clean      Limpa a pasta de build antes de compilar"
    echo "  --clean    Limpa a pasta de build antes de compilar"
    echo "  -c         Limpa a pasta de build antes de compilar"
    echo "  help       Exibe esta mensagem de ajuda"
    echo ""
    echo "Exemplos:"
    echo "  ./build.sh          # Compila normalmente"
    echo "  ./build.sh clean    # Limpa a pasta build e recompila do zero"
}

# Verificar se foi solicitada limpeza
DO_CLEAN=false
for arg in "$@"; do
    case "$arg" in
        clean|--clean|-c)
            DO_CLEAN=true
            ;;
        help|--help|-h)
            show_help
            exit 0
            ;;
        *)
            echo "Opção desconhecida: $arg"
            show_help
            exit 1
            ;;
    esac
done

if [ "$DO_CLEAN" = true ]; then
    echo "==> Limpando diretório de build '$BUILD_DIR'..."
    rm -rf "$BUILD_DIR"
    echo "==> Diretório de build limpo com sucesso."
fi

# Detectar ambiente e garantir MinGW64 no PATH caso esteja no MSYS2
if [ -d "/d/msys64/mingw64/bin" ]; then
    export PATH="/d/msys64/mingw64/bin:$PATH"
elif [ -d "D:/msys64/mingw64/bin" ]; then
    export PATH="D:/msys64/mingw64/bin:$PATH"
fi

# Criar pastas necessárias
mkdir -p "$BUILD_DIR"
mkdir -p "$PROJECT_DIR/$DLL_DIR"

cd "$BUILD_DIR"

echo "==> Configurando CMake (MinGW Makefiles)..."
if command -v mingw32-make >/dev/null 2>&1; then
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
elif command -v make >/dev/null 2>&1; then
    cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..
else
    cmake ..
fi

echo "==> Compilando o projeto..."
cmake --build . --config Release -j$(nproc 2>/dev/null || echo 4)

# Copiar DLLs do MinGW necessárias para a pasta dll
echo "==> Verificando dependências de DLLs..."

MINGW_BIN=""

if [ -d "/d/msys64/mingw64/bin" ]; then
    MINGW_BIN="/d/msys64/mingw64/bin"
elif [ -d "D:/msys64/mingw64/bin" ]; then
    MINGW_BIN="D:/msys64/mingw64/bin"
fi

if [ -n "$MINGW_BIN" ]; then
    for dll in \
        libcurl-4.dll \
        libbrotlidec.dll \
        libbrotlicommon.dll \
        libcrypto-3-x64.dll \
        libssl-3-x64.dll \
        libidn2-0.dll \
        libpsl-5.dll \
        libssh2-1.dll \
        libnghttp2-14.dll \
        libnghttp3-9.dll \
        libngtcp2-16.dll \
        libngtcp2_crypto_ossl-0.dll \
        libzstd.dll \
        zlib1.dll \
        libiconv-2.dll \
        libintl-8.dll \
        libunistring-5.dll \
        libwinpthread-1.dll
    do
        if [ -f "$MINGW_BIN/$dll" ]; then
            cp "$MINGW_BIN/$dll" "$PROJECT_DIR/$DLL_DIR/" 2>/dev/null || true
        fi
    done
fi

# Garantir yt-dlp.exe na raiz do projeto
if [ ! -f "$PROJECT_DIR/yt-dlp.exe" ] && \
   [ -n "$MINGW_BIN" ] && \
   [ -f "$MINGW_BIN/yt-dlp.exe" ]; then
    cp "$MINGW_BIN/yt-dlp.exe" "$PROJECT_DIR/" 2>/dev/null || true
fi

# Garantir pasta de downloads na raiz do projeto
mkdir -p "$PROJECT_DIR/downloads"

echo ""
echo "=========================================================="
echo " [SUCESSO] Build concluído com sucesso!"
echo " Executável: $PROJECT_DIR/VideoDownloader.exe"
echo " DLLs:       $PROJECT_DIR/$DLL_DIR/"
echo " Downloads:  $PROJECT_DIR/downloads/"
echo "=========================================================="