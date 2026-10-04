# Guia de Compilação e Uso - Video Downloader (C++ / MinGW64)

Este projeto foi desenvolvido em **C++17** com suporte a ambiente **MinGW-w64** (como o fornecido pelo **MSYS2**). Ele permite baixar vídeos informando uma URL direta ou a partir de um arquivo de configuração `.ini`, exibindo progresso em tempo real (porcentagem, tamanho transferido e velocidade). Possui tanto **modo Terminal (CLI)** quanto uma **Interface Gráfica completa (GUI)** com player de vídeo integrado para assistir aos arquivos baixados.

---

## 1. Estrutura do Projeto

```text
videoDownloader/
├── CMakeLists.txt        # Configuração do CMake
├── build.sh              # Script Bash para automação de build (com suporte a clean)
├── compilar.md           # Este manual de instruções
├── urls_exemplo.ini      # Exemplo de arquivo .ini contendo URLs para download
├── include/              # Cabeçalhos C++ (.h)
│   ├── Downloader.h      # Módulo de download cURL com progresso e velocidade
│   ├── IniParser.h       # Analisador de arquivos .ini
│   ├── MainWindow.h      # Janela e interface gráfica nativa Win32
│   └── VideoPlayer.h     # Reprodutor de vídeo nativo DirectShow
├── src/                  # Código-fonte C++ (.cpp)
│   ├── main.cpp          # Ponto de entrada (CLI e GUI)
│   ├── Downloader.cpp    # Implementação do motor de download
│   ├── IniParser.cpp     # Implementação do parser INI
│   ├── MainWindow.cpp    # Implementação dos controles e eventos da GUI
│   └── VideoPlayer.cpp   # Implementação do controle de reprodução de vídeo
└── downloads/            # Diretório de destino padrão dos vídeos baixados
```

---

## 2. Requisitos de Ambiente (MinGW-w64)

Para compilar o projeto em ambiente MinGW-w64 (MSYS2):

1. **GCC / G++** (`mingw-w64-x86_64-gcc`)
2. **CMake** (`mingw-w64-x86_64-cmake`)
3. **Make** (`mingw-w64-x86_64-make`)
4. **libcurl** (`mingw-w64-x86_64-curl`)

Se estiver instalando pacotes no MSYS2 MinGW64:
```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-curl
```

---

## 3. Como Compilar

### Opção A: Utilizando o script `build.sh` (Recomendado)

O script `build.sh` compila o projeto automaticamente e aceita opções de limpeza.

#### 1. Compilação Normal:
No terminal MSYS2 MinGW64 ou Git Bash:
```bash
./build.sh
```

#### 2. Limpar a pasta de build e recompilar do zero:
```bash
./build.sh clean
```
*(Também aceita `--clean` ou `-c`)*

---

### Opção B: Compilação Manual via CMake

No terminal do MinGW64:
```bash
mkdir -p build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

---

## 4. Como Executar

O executável gerado será: `build/VideoDownloader.exe` (ou `./VideoDownloader.exe` ao entrar na pasta `build`).

### 1. Interface Gráfica (GUI)
Para abrir a interface gráfica com o reprodutor de vídeo e controles:
```bash
./build/VideoDownloader.exe
```
ou
```bash
./build/VideoDownloader.exe --gui
```

#### Recursos da GUI:
- **Download por URL direta**: Digite o link do vídeo no campo de texto e clique em **Baixar URL**.
- **Download por arquivo `.ini`**: Clique em **Baixar de .INI...** e selecione qualquer arquivo `.ini` com URLs.
- **Progresso em tempo real**: Exibe barra de progresso visual, porcentagem, total baixado e velocidade em KB/s ou MB/s.
- **Botão de Cancelar**: Permite abortar o download em andamento de forma segura.
- **Lista de Vídeos Baixados**: Lista todos os arquivos presentes na pasta `downloads/`. Um duplo clique ou seleção reproduz o vídeo.
- **Reprodutor de Vídeo Integrado**: Botões de ▶ Reproduzir, ⏸ Pausar e ⏹ Parar diretamente na janela.
- **Abrir Pasta**: Abre a pasta de arquivos baixados diretamente no Windows Explorer.

---

### 2. Modo Terminal (CLI)

O aplicativo pode ser executado diretamente pelo terminal para automações ou scripts:

#### A) Baixar a partir de um arquivo `.ini`:
```bash
./build/VideoDownloader.exe -i urls_exemplo.ini
```

#### B) Baixar uma URL direta:
```bash
./build/VideoDownloader.exe -u "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ForBiggerBlazes.mp4"
```
Ou simplesmente passando a URL como argumento:
```bash
./build/VideoDownloader.exe "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ForBiggerBlazes.mp4"
```

#### C) Definir pasta de saída personalizada:
```bash
./build/VideoDownloader.exe -i urls_exemplo.ini -o "minha_pasta_de_videos"
```

#### D) Exibir ajuda do terminal:
```bash
./build/VideoDownloader.exe --help
```

---

## 5. Formato do Arquivo `.ini`

O arquivo `.ini` pode conter seções livres e qualquer chave que represente uma URL, ou mesmo URLs diretas:

```ini
[videos]
video1 = https://servidor.com/video1.mp4
video2 = https://servidor.com/aula.mkv

[colecao]
url = https://servidor.com/filme.avi
```
