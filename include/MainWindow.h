#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "Downloader.h"
#include "VideoPlayer.h"

// Mensagens personalizadas para comunicação de threads com a GUI
#define WM_APP_DOWNLOAD_PROGRESS (WM_USER + 101)
#define WM_APP_DOWNLOAD_FINISHED (WM_USER + 102)
#define WM_APP_DOWNLOAD_LOG (WM_USER + 103)

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool create(HINSTANCE hInstance, int nCmdShow);
    int run();

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void createControls(HWND hWnd);
    void updateLayout(int width, int height);
    void refreshDownloadedList();
    void playSelectedVideo();
    void startDownloadUrl();
    void startDownloadIni();
    void cancelDownload();
    void openContainingFolder();

    HINSTANCE m_hInstance;
    HWND m_hWnd;

    // Controles GUI
    HWND m_hEditUrl;
    HWND m_hBtnDownloadUrl;
    HWND m_hBtnDownloadIni;
    HWND m_hBtnCancel;
    HWND m_hProgressBar;
    HWND m_hLabelStatus;
    HWND m_hLabelSpeed;
    HWND m_hTreeVideos;      // TreeView hierárquico com pastas/playlists e vídeos
    HWND m_hVideoPanel;
    HWND m_hBtnPlay;
    HWND m_hBtnPause;
    HWND m_hBtnStop;
    HWND m_hBtnOpenFolder;
    HWND m_hBtnRefreshList;

    // Fontes para visual moderno
    HFONT m_hFontUI;
    HFONT m_hFontBold;

    // Gerenciador de download e player
    std::unique_ptr<Downloader> m_downloader;
    std::unique_ptr<VideoPlayer> m_videoPlayer;
    std::thread m_downloadThread;
    std::atomic<bool> m_isDownloading;

    std::string m_downloadsDir;
    std::vector<std::string> m_videoFilePaths;
    std::vector<std::string> m_treeItemPaths; // Mapeamento item -> caminho absoluto/relativo
};
#endif // MAIN_WINDOW_H
