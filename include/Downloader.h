#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>
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
    int currentItemIndex;    // Para múltiplos vídeos / playlist
    int totalItems;          // Total de vídeos na playlist
    std::string currentItemTitle;
};

struct PlaylistInfo {
    bool isPlaylist;
    std::string title;
    std::string playlistId;
    std::vector<std::string> videoUrls;
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

    // Faz o download de uma URL direta para um arquivo destino
    bool download(const std::string& url,
                  const std::string& destinationPath,
                  ProgressCallback onProgress = nullptr,
                  StatusCallback onStatus = nullptr);

    // Faz o download de uma URL genérica ou Playlist
    // Se for playlist, cria automaticamente uma subpasta com o nome da playlist dentro de baseFolder
    bool downloadTarget(const std::string& url,
                        const std::string& baseFolder,
                        ProgressCallback onProgress = nullptr,
                        StatusCallback onStatus = nullptr);

    // Detecção e inspeção de playlist
    static PlaylistInfo inspectPlaylist(const std::string& url);

    // Sanear nomes de pastas para o sistema de arquivos do Windows
    static std::string sanitizeFilename(const std::string& name);

    // Utilitário para extrair nome de arquivo da URL
    static std::string getFilenameFromUrl(const std::string& url);

    // Utilitário para formatar bytes em KB, MB, GB
    static std::string formatBytes(double bytes);

    // Utilitário HTTP GET em string
    static std::string fetchString(const std::string& url);

    void cancel();
    bool isCancelled() const;

private:
    bool downloadWithYtDlp(const std::string& url,
                           const std::string& targetFolder,
                           bool isPlaylist,
                           ProgressCallback onProgress,
                           StatusCallback onStatus);

    static std::string findYtDlpExecutable();

    bool m_cancelled;
    double m_lastDownloadedBytes;
    double m_lastTimeSec;
    double m_currentSpeed;
};

#endif // DOWNLOADER_H
