#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <functional>
#include <cstdint>

struct DownloadProgress {
    double totalBytes;
    double downloadedBytes;
    double percentage;
    double speedBytesPerSec; // Bytes por segundo
    std::string speedFormatted;
    std::string downloadedFormatted;
    std::string totalFormatted;
};

// Callback de progresso: retorna false se o usuário quiser cancelar
using ProgressCallback = std::function<bool(const DownloadProgress& progress)>;
using StatusCallback = std::function<void(const std::string& statusMessage)>;

class Downloader {
public:
    Downloader();
    ~Downloader();

    // Inicialização global de rede (curl)
    static void initNetwork();
    static void cleanupNetwork();

    // Faz o download de uma URL para um arquivo destino
    bool download(const std::string& url, 
                  const std::string& destinationPath,
                  ProgressCallback onProgress = nullptr,
                  StatusCallback onStatus = nullptr);

    // Utilitário para extrair nome de arquivo da URL
    static std::string getFilenameFromUrl(const std::string& url);

    // Utilitário para formatar bytes em KB, MB, GB
    static std::string formatBytes(double bytes);

    void cancel();
    bool isCancelled() const;

private:
    bool m_cancelled;
    double m_lastDownloadedBytes;
    double m_lastTimeSec;
    double m_currentSpeed;
};
#endif // DOWNLOADER_H
