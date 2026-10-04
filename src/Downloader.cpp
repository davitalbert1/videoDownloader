#include "Downloader.h"
#include <curl/curl.h>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace {
    struct ProgressData {
        Downloader* downloader;
        ProgressCallback callback;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastTime;
        double lastDownloadedBytes;
    };

    size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
        return fwrite(ptr, size, nmemb, stream);
    }

    int xferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        (void)ultotal;
        (void)ulnow;
        auto* data = static_cast<ProgressData*>(clientp);
        if (!data || !data->downloader) return 0;

        if (data->downloader->isCancelled()) return 1; // Cancela libcurl (CURLE_ABORTED_BY_CALLBACK)

        if (data->callback && dlnow > 0) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsedSinceLast = now - data->lastTime;

            double speed = 0.0;
            if (elapsedSinceLast.count() >= 0.25) {
                speed = (static_cast<double>(dlnow) - data->lastDownloadedBytes) / elapsedSinceLast.count();
                data->lastDownloadedBytes = static_cast<double>(dlnow);
                data->lastTime = now;
            } else {
                std::chrono::duration<double> totalElapsed = now - data->startTime;
                if (totalElapsed.count() > 0.05) {
                    speed = static_cast<double>(dlnow) / totalElapsed.count();
                }
            }

            DownloadProgress progress;
            progress.totalBytes = static_cast<double>(dltotal);
            progress.downloadedBytes = static_cast<double>(dlnow);
            progress.percentage = (dltotal > 0) ? (static_cast<double>(dlnow) / static_cast<double>(dltotal) * 100.0) : 0.0;
            progress.speedBytesPerSec = speed;
            progress.speedFormatted = Downloader::formatBytes(speed) + "/s";
            progress.downloadedFormatted = Downloader::formatBytes(progress.downloadedBytes);
            progress.totalFormatted = (dltotal > 0) ? Downloader::formatBytes(progress.totalBytes) : "Desconhecido";

            bool continueDownload = data->callback(progress);
            if (!continueDownload) {
                data->downloader->cancel();
                return 1;
            }
        }

        return 0;
    }
}

Downloader::Downloader() 
    : m_cancelled(false), m_lastDownloadedBytes(0.0), m_lastTimeSec(0.0), m_currentSpeed(0.0) {
}

Downloader::~Downloader() {
}

void Downloader::initNetwork() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void Downloader::cleanupNetwork() {
    curl_global_cleanup();
}

void Downloader::cancel() {
    m_cancelled = true;
}

bool Downloader::isCancelled() const {
    return m_cancelled;
}

std::string Downloader::formatBytes(double bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes >= 1024.0 * 1024.0 * 1024.0) {
        ss << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else if (bytes >= 1024.0 * 1024.0) {
        ss << (bytes / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024.0) {
        ss << (bytes / 1024.0) << " KB";
    } else {
        ss << static_cast<long long>(bytes) << " B";
    }
    return ss.str();
}

std::string Downloader::getFilenameFromUrl(const std::string& url) {
    std::string cleanUrl = url;
    size_t queryPos = cleanUrl.find('?');
    if (queryPos != std::string::npos) cleanUrl = cleanUrl.substr(0, queryPos);
    size_t hashPos = cleanUrl.find('#');
    if (hashPos != std::string::npos) cleanUrl = cleanUrl.substr(0, hashPos);

    size_t slashPos = cleanUrl.find_last_of("/\\");
    std::string filename = (slashPos != std::string::npos) ? cleanUrl.substr(slashPos + 1) : cleanUrl;

    if (filename.empty()) filename = "video_download.mp4";

    // Se não tiver extensão de vídeo comum, adiciona .mp4 como fallback
    if (filename.find('.') == std::string::npos) filename += ".mp4";

    return filename;
}

bool Downloader::download(const std::string& url, 
                         const std::string& destinationPath,
                         ProgressCallback onProgress,
                         StatusCallback onStatus) {
    m_cancelled = false;

    if (onStatus) onStatus("Iniciando conexão com " + url + "...");

    FILE* fp = fopen(destinationPath.c_str(), "wb");
    if (!fp) {
        if (onStatus) onStatus("Erro: Não foi possível criar o arquivo de saída.");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        if (onStatus) onStatus("Erro: Falha ao inicializar o cURL.");
        return false;
    }

    ProgressData progressData;
    progressData.downloader = this;
    progressData.callback = onProgress;
    progressData.startTime = std::chrono::steady_clock::now();
    progressData.lastTime = progressData.startTime;
    progressData.lastDownloadedBytes = 0.0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Seguir redirecionamentos HTTP 301/302
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) VideoDownloader/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Para flexibilidade em ambientes sem bundle CA local
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Falhar em HTTP 4xx e 5xx

    CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        // Remover arquivo incompleto se falhou ou foi cancelado
        remove(destinationPath.c_str());
        if (onStatus) {
            if (m_cancelled || res == CURLE_ABORTED_BY_CALLBACK) {
                onStatus("Download cancelado pelo usuário.");
            } else {
                onStatus(std::string("Erro no download: ") + curl_easy_strerror(res));
            }
        }
        return false;
    }

    if (onStatus) onStatus("Download concluído com sucesso!");

    return true;
}
