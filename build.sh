#!/usr/bin/env bash

# ==============================================================================
# Script de automação de build para o VideoDownloader (MinGW-w64 + CMake)
# ==============================================================================

set -e

BUILD_DIR="build"

# Função para exibir ajuda
show_help() {
    echo "Uso: ./build.sh [OPCOES]"
    echo ""
    echo "Opcoes:"
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
            echo "Opcao desconhecida: $arg"
            show_help
            exit 1
            ;;
    esac
done

if [ "$DO_CLEAN" = true ]; then
    echo "==> Limpando diretorio de build '$BUILD_DIR'..."
    rm -rf "$BUILD_DIR"
    echo "==> Diretorio de build limpo com sucesso."
fi

# Detectar ambiente e garantir MinGW64 no PATH caso esteja no MSYS2
if [ -d "/d/msys64/mingw64/bin" ]; then
    export PATH="/d/msys64/mingw64/bin:$PATH"
elif [ -d "D:/msys64/mingw64/bin" ]; then
    export PATH="D:/msys64/mingw64/bin:$PATH"
fi

# Criar pasta de build se não existir
mkdir -p "$BUILD_DIR"
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

# Copiar DLLs do MinGW necessárias para a pasta do executável (se existirem no PATH do MinGW)
echo "==> Verificando dependencias de DLLs..."
MINGW_BIN=""
if [ -d "/d/msys64/mingw64/bin" ]; then
    MINGW_BIN="/d/msys64/mingw64/bin"
elif [ -d "D:/msys64/mingw64/bin" ]; then
    MINGW_BIN="D:/msys64/mingw64/bin"
fi

if [ -n "$MINGW_BIN" ]; then
    for dll in libcurl-4.dll libbrotlidec.dll libbrotlicommon.dll libcrypto-3-x64.dll \
               libssl-3-x64.dll libidn2-0.dll libpsl-5.dll libssh2-1.dll libnghttp2-14.dll \
               libnghttp3-9.dll libngtcp2-16.dll libngtcp2_crypto_ossl-0.dll libzstd.dll \
               zlib1.dll libiconv-2.dll libintl-8.dll libunistring-5.dll; do
        if [ -f "$MINGW_BIN/$dll" ] && [ ! -f "$dll" ]; then
            cp "$MINGW_BIN/$dll" . 2>/dev/null || true
        fi
    done
fi

# Garantir pasta de downloads no diretório do executável
mkdir -p downloads

echo ""
echo "=========================================================="
echo " [SUCESSO] Build concluido com sucesso!"
echo " Executavel gerado em: $BUILD_DIR/VideoDownloader.exe"
echo "=========================================================="
