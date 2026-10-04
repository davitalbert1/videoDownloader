#include "Downloader.h"
#include <curl/curl.h>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace {
    struct ProgressData {
        Downloader* downloader;
        ProgressCallback callback;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastTime;
        double lastDownloadedBytes;
        int currentItemIndex;
        int totalItems;
        std::string title;
    };

    size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
        return fwrite(ptr, size, nmemb, stream);
    }

    size_t writeStringCallback(void* ptr, size_t size, size_t nmemb, std::string* s) {
        size_t total = size * nmemb;
        s->append(static_cast<char*>(ptr), total);
        return total;
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

            // Limitar a frequência de atualização do callback para no máximo a cada 100ms
            if (elapsedSinceLast.count() < 0.10 && dlnow < dltotal) {
                return 0;
            }

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
            progress.currentItemIndex = data->currentItemIndex;
            progress.totalItems = data->totalItems;
            progress.currentItemTitle = data->title;

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

std::string Downloader::sanitizeFilename(const std::string& name) {
    std::string safe = name;
    const std::string invalidChars = "\\/:*?\"<>|";
    for (char& c : safe) {
        if (invalidChars.find(c) != std::string::npos || static_cast<unsigned char>(c) < 32) {
            c = '_';
        }
    }
    size_t first = safe.find_first_not_of(" ._");
    if (first == std::string::npos) return "playlist";
    size_t last = safe.find_last_not_of(" ._");
    safe = safe.substr(first, (last - first + 1));
    if (safe.empty()) return "playlist";
    return safe;
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

    return sanitizeFilename(filename);
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
    progressData.currentItemIndex = 1;
    progressData.totalItems = 1;
    progressData.title = getFilenameFromUrl(url);

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

std::string Downloader::fetchString(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) VideoDownloader/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return response;
}

PlaylistInfo Downloader::inspectPlaylist(const std::string& url) {
    PlaylistInfo info;
    info.isPlaylist = false;

    // 1. YouTube Playlist (parâmetro 'list=')
    size_t listPos = url.find("list=");
    if (listPos != std::string::npos && (url.find("youtube.com") != std::string::npos || url.find("youtu.be") != std::string::npos)) {
        info.isPlaylist = true;
        std::string listId = url.substr(listPos + 5);
        size_t ampPos = listId.find('&');
        if (ampPos != std::string::npos) {
            listId = listId.substr(0, ampPos);
        }
        info.playlistId = listId;
        info.title = "Playlist_" + listId;

        std::string playlistUrl = "https://www.youtube.com/playlist?list=" + listId;
        std::string html = fetchString(playlistUrl);
        if (!html.empty()) {
            std::regex titleRegex("<title>(.*?)(?: - YouTube)?</title>", std::regex::icase);
            std::smatch titleMatch;
            if (std::regex_search(html, titleMatch, titleRegex) && titleMatch.size() > 1) {
                std::string parsedTitle = titleMatch[1].str();
                if (!parsedTitle.empty() && parsedTitle != "YouTube") {
                    info.title = sanitizeFilename(parsedTitle);
                }
            }

            std::regex vidRegex("/watch\\?v=([a-zA-Z0-9_-]{11})");
            auto words_begin = std::sregex_iterator(html.begin(), html.end(), vidRegex);
            auto words_end = std::sregex_iterator();
            std::vector<std::string> seenIds;

            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::smatch match = *i;
                std::string vidId = match[1].str();
                if (std::find(seenIds.begin(), seenIds.end(), vidId) == seenIds.end()) {
                    seenIds.push_back(vidId);
                    info.videoUrls.push_back("https://www.youtube.com/watch?v=" + vidId);
                }
            }
        }
        return info;
    }

    // 2. Playlist M3U8 ou lista em arquivo .m3u8 / .m3u
    std::string lowerUrl = url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    if (lowerUrl.find(".m3u8") != std::string::npos || lowerUrl.find(".m3u") != std::string::npos) {
        info.isPlaylist = true;
        info.title = "M3U_Playlist";
        info.playlistId = "m3u";

        std::string content = fetchString(url);
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty() || line[0] == '#') continue;

            if (line.rfind("http://", 0) == 0 || line.rfind("https://", 0) == 0) {
                info.videoUrls.push_back(line);
            }
        }
        return info;
    }

    return info;
}

std::string Downloader::findYtDlpExecutable() {
    std::vector<std::string> candidates = {
        "yt-dlp.exe",
        "./yt-dlp.exe",
        "../yt-dlp.exe",
        "build/yt-dlp.exe",
        "D:/msys64/mingw64/bin/yt-dlp.exe"
    };

    for (const auto& path : candidates) {
        if (fs::exists(path)) {
            try {
                return fs::absolute(path).string();
            } catch (...) {
                return path;
            }
        }
    }

    char searchBuf[MAX_PATH];
    DWORD res = SearchPathA(NULL, "yt-dlp.exe", NULL, MAX_PATH, searchBuf, NULL);
    if (res > 0) return std::string(searchBuf);

    return "";
}

bool Downloader::downloadWithYtDlp(const std::string& url,
                                   const std::string& targetFolder,
                                   bool isPlaylist,
                                   ProgressCallback onProgress,
                                   StatusCallback onStatus) {
    std::string ytDlp = findYtDlpExecutable();
    if (ytDlp.empty()) {
        if (onStatus) onStatus("Tentando obter motor yt-dlp para transmissões de vídeo...");
        std::string ytDlpUrl = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
        bool dlOk = download(ytDlpUrl, "yt-dlp.exe");
        if (dlOk) {
            ytDlp = findYtDlpExecutable();
            if (ytDlp.empty()) ytDlp = "yt-dlp.exe";
        }
    }

    if (ytDlp.empty()) {
        if (onStatus) onStatus("Aviso: yt-dlp não disponível para vídeos de streaming complexo.");
        return false;
    }

    // Detectar runtime JavaScript disponível (Node.js, Deno, Bun, QuickJS)
    std::string jsRuntimeArg = "";
    char jsPathBuf[MAX_PATH];
    if (SearchPathA(NULL, "deno.exe", NULL, MAX_PATH, jsPathBuf, NULL) > 0) {
        jsRuntimeArg = "--js-runtimes deno ";
    } else if (SearchPathA(NULL, "node.exe", NULL, MAX_PATH, jsPathBuf, NULL) > 0) {
        jsRuntimeArg = "--js-runtimes node ";
    } else if (SearchPathA(NULL, "bun.exe", NULL, MAX_PATH, jsPathBuf, NULL) > 0) {
        jsRuntimeArg = "--js-runtimes bun ";
    } else if (SearchPathA(NULL, "quickjs.exe", NULL, MAX_PATH, jsPathBuf, NULL) > 0) {
        jsRuntimeArg = "--js-runtimes quickjs ";
    }

    // Windows cmd.exe /c com _popen requer envolver todo o comando entre aspas externas
    // formato: ""caminho\yt-dlp.exe" arg1 "arg 2" "arg 3""
    std::ostringstream cmd;
    cmd << "\"\"" << ytDlp << "\" "
        << "--newline "
        << "--no-mtime "
        << "--no-warnings "
        << jsRuntimeArg
        << "-f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\" "
        << "-o \"" << targetFolder << "/%(title)s.%(ext)s\" ";
    if (isPlaylist) {
        cmd << "--yes-playlist ";
    } else {
        cmd << "--no-playlist ";
    }
    cmd << "\"" << url << "\"\"";

    if (onStatus) onStatus("Executando motor yt-dlp...");

    FILE* pipe = _popen(cmd.str().c_str(), "r");
    if (!pipe) return false;

    char buffer[512];
    std::regex pctRegex(R"(\[download\]\s+([0-9.]+)%\s+of\s+([~0-9.]+[A-Za-z]+)\s+at\s+([0-9.]+[A-Za-z/]+))");

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (m_cancelled) {
            break;
        }

        std::string line(buffer);
        std::smatch match;
        if (std::regex_search(line, match, pctRegex) && match.size() >= 4) {
            double pct = std::stod(match[1].str());
            DownloadProgress prog = {};
            prog.percentage = pct;
            prog.speedFormatted = match[3].str();
            prog.totalFormatted = match[2].str();
            prog.downloadedFormatted = "";
            prog.currentItemIndex = 1;
            prog.totalItems = 1;
            if (onProgress) onProgress(prog);
        } else {
            if (line.find("[download] Destination:") != std::string::npos) {
                if (onStatus) onStatus(line);
            }
        }
    }

    int res = _pclose(pipe);
    return (res == 0);
}

bool Downloader::downloadTarget(const std::string& url,
                               const std::string& baseFolder,
                               ProgressCallback onProgress,
                               StatusCallback onStatus) {
    m_cancelled = false;

    // 1. Detectar se a URL é de uma Playlist
    PlaylistInfo pInfo = inspectPlaylist(url);

    if (pInfo.isPlaylist) {
        std::string playlistFolder = baseFolder + "/" + pInfo.title;
        fs::create_directories(playlistFolder);

        if (onStatus) {
            onStatus("Playlist detectada: '" + pInfo.title + "'. Criando pasta...");
        }

        if (!findYtDlpExecutable().empty() || (url.find("youtube.com") != std::string::npos || url.find("youtu.be") != std::string::npos)) {
            bool ok = downloadWithYtDlp(url, playlistFolder, true, onProgress, onStatus);
            if (ok) return true;
        }

        if (!pInfo.videoUrls.empty()) {
            int total = static_cast<int>(pInfo.videoUrls.size());
            int successCount = 0;

            for (size_t i = 0; i < pInfo.videoUrls.size(); ++i) {
                if (m_cancelled) break;

                const std::string& vUrl = pInfo.videoUrls[i];
                std::string filename = getFilenameFromUrl(vUrl);
                std::string destPath = playlistFolder + "/" + filename;

                if (onStatus) {
                    std::ostringstream ss;
                    ss << "[" << (i + 1) << "/" << total << "] Baixando: " << filename;
                    onStatus(ss.str());
                }

                bool downloaded = false;
                if (vUrl.find("youtube.com") != std::string::npos || vUrl.find("youtu.be") != std::string::npos) {
                    downloaded = downloadWithYtDlp(vUrl, playlistFolder, false, onProgress, onStatus);
                } else {
                    downloaded = download(vUrl, destPath, onProgress, onStatus);
                }

                if (downloaded) successCount++;
            }

            return (successCount > 0);
        }

        return false;
    }

    // 2. Se for link de vídeo YouTube único
    if (url.find("youtube.com") != std::string::npos || url.find("youtu.be") != std::string::npos) {
        bool ok = downloadWithYtDlp(url, baseFolder, false, onProgress, onStatus);
        if (ok) return true;
    }

    // 3. Download direto via cURL
    std::string filename = getFilenameFromUrl(url);
    std::string destPath = baseFolder + "/" + filename;
    return download(url, destPath, onProgress, onStatus);
}
