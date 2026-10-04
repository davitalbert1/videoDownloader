#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <csignal>
#include "Downloader.h"
#include "IniParser.h"
#include "MainWindow.h"

namespace fs = std::filesystem;

static Downloader* g_currentDownloader = nullptr;

void signalHandler(int signum) {
    if (g_currentDownloader) {
        std::cout << "\n[!] Interrupcao recebida (Ctrl+C). Cancelando download..." << std::endl;
        g_currentDownloader->cancel();
    }
}

void printCliProgressBar(const DownloadProgress& p) {
    const int barWidth = 40;
    float ratio = (p.totalBytes > 0) ? static_cast<float>(p.downloadedBytes / p.totalBytes) : 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int pos = static_cast<int>(barWidth * ratio);

    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << static_cast<int>(p.percentage) << "% "
              << p.downloadedFormatted << " / " << p.totalFormatted
              << " (" << p.speedFormatted << ")   " << std::flush;
}

void printUsage(const char* progName) {
    std::cout << "========================================================\n"
              << "             VIDEO DOWNLOADER & PLAYER\n"
              << "========================================================\n"
              << "Uso:\n"
              << "  " << progName << "                       (Inicia com Interface Grafica)\n"
              << "  " << progName << " --gui                 (Inicia com Interface Grafica)\n"
              << "  " << progName << " <URL>                 (Baixa o video direto via terminal)\n"
              << "  " << progName << " -u <URL> [-o pasta]   (Baixa video via terminal para pasta)\n"
              << "  " << progName << " -i <arquivo.ini>      (Baixa todos os videos do arquivo INI)\n"
              << "  " << progName << " --help, -h            (Exibe esta ajuda)\n"
              << "========================================================\n";
}

int runCli(const std::vector<IniEntry>& entries, const std::string& outputFolder) {
    fs::create_directories(outputFolder);
    signal(SIGINT, signalHandler);

    Downloader::initNetwork();
    Downloader downloader;
    g_currentDownloader = &downloader;

    int total = static_cast<int>(entries.size());
    int successCount = 0;

    std::cout << "\nIniciando processamento de " << total << " item(ns) para '" << outputFolder << "'...\n\n";

    for (size_t i = 0; i < entries.size(); ++i) {
        if (downloader.isCancelled()) {
            std::cout << "\nOperacao cancelada.\n";
            break;
        }

        const auto& entry = entries[i];
        std::string targetFolder = outputFolder;
        if (!entry.customFolder.empty()) {
            targetFolder += "/" + Downloader::sanitizeFilename(entry.customFolder);
            fs::create_directories(targetFolder);
        }

        std::cout << "[" << (i + 1) << "/" << total << "] Processando: " << entry.url << "\n";
        if (!entry.customFolder.empty()) {
            std::cout << " -> Pasta de destino: " << targetFolder << "\n";
        }

        bool ok = downloader.downloadTarget(
            entry.url,
            targetFolder,
            [](const DownloadProgress& p) -> bool {
                printCliProgressBar(p);
                return true;
            },
            [](const std::string& status) {
                std::cout << " -> " << status << "\n";
            }
        );

        std::cout << "\n";
        if (ok) {
            std::cout << "=> [SUCESSO] Concluido com sucesso!\n\n";
            successCount++;
        } else {
            std::cout << "=> [FALHA] Nao foi possivel concluir o download.\n\n";
        }
    }

    g_currentDownloader = nullptr;
    Downloader::cleanupNetwork();

    std::cout << "Resumo: " << successCount << " de " << total << " operacao(oes) concluidas com sucesso.\n";
    return (successCount == total) ? 0 : 1;
}

int runCli(const std::vector<std::string>& urls, const std::string& outputFolder) {
    std::vector<IniEntry> entries;
    for (const auto& u : urls) {
        IniEntry e;
        e.url = u;
        entries.push_back(e);
    }
    return runCli(entries, outputFolder);
}

int main(int argc, char* argv[]) {
    std::string iniFile = "";
    std::string directUrl = "";
    std::string outputDir = "downloads";
    bool forceGui = false;

    if (argc == 1) {
        forceGui = true;
    } else {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--gui" || arg == "-g") {
                forceGui = true;
            } else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else if (arg == "-i" || arg == "--ini") {
                if (i + 1 < argc) {
                    iniFile = argv[++i];
                } else {
                    std::cerr << "Erro: opcao " << arg << " requer o caminho do arquivo .ini\n";
                    return 1;
                }
            } else if (arg == "-u" || arg == "--url") {
                if (i + 1 < argc) {
                    directUrl = argv[++i];
                } else {
                    std::cerr << "Erro: opcao " << arg << " requer uma URL\n";
                    return 1;
                }
            } else if (arg == "-o" || arg == "--out") {
                if (i + 1 < argc) {
                    outputDir = argv[++i];
                } else {
                    std::cerr << "Erro: opcao " << arg << " requer o caminho de saida\n";
                    return 1;
                }
            } else if (arg.rfind("http://", 0) == 0 || arg.rfind("https://", 0) == 0) {
                directUrl = arg;
            } else if (arg.size() > 4 && arg.substr(arg.size() - 4) == ".ini") {
                iniFile = arg;
            } else {
                std::cerr << "Parametro desconhecido: " << arg << "\n";
                printUsage(argv[0]);
                return 1;
            }
        }
    }

    // Modo Terminal com arquivo .INI
    if (!iniFile.empty()) {
        IniParser parser;
        if (!parser.loadFromFile(iniFile)) {
            std::cerr << "Erro: Nao foi possivel abrir o arquivo INI: " << iniFile << "\n";
            return 1;
        }
        std::vector<IniEntry> entries = parser.getEntries();
        if (entries.empty()) {
            std::cerr << "Aviso: Nenhuma URL encontrada no arquivo " << iniFile << "\n";
            return 1;
        }
        return runCli(entries, outputDir);
    }

    // Modo Terminal com URL direta
    if (!directUrl.empty()) return runCli({directUrl}, outputDir);

    // Modo Interface Gráfica (Padrão ou --gui)
    Downloader::initNetwork();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    MainWindow appWindow;
    if (!appWindow.create(hInstance, SW_SHOWDEFAULT)) {
        std::cerr << "Erro ao inicializar a interface grafica.\n";
        Downloader::cleanupNetwork();
        return 1;
    }

    int exitCode = appWindow.run();

    Downloader::cleanupNetwork();
    return exitCode;
}
