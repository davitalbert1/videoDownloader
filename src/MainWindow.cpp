#include "MainWindow.h"
#include "IniParser.h"
#include <commdlg.h>
#include <shellapi.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// IDs dos Controles
enum ControlIDs {
    IDC_EDIT_URL = 1001,
    IDC_BTN_DOWNLOAD_URL,
    IDC_BTN_DOWNLOAD_INI,
    IDC_BTN_CANCEL,
    IDC_PROGRESS_BAR,
    IDC_LABEL_STATUS,
    IDC_LABEL_SPEED,
    IDC_LIST_VIDEOS,
    IDC_VIDEO_PANEL,
    IDC_BTN_PLAY,
    IDC_BTN_PAUSE,
    IDC_BTN_STOP,
    IDC_BTN_REFRESH,
    IDC_BTN_OPEN_FOLDER
};

struct ProgressPayload {
    double percentage;
    std::string speedStr;
    std::string progressStr;
};

MainWindow::MainWindow()
    : m_hInstance(nullptr),
      m_hWnd(nullptr),
      m_hEditUrl(nullptr),
      m_hBtnDownloadUrl(nullptr),
      m_hBtnDownloadIni(nullptr),
      m_hBtnCancel(nullptr),
      m_hProgressBar(nullptr),
      m_hLabelStatus(nullptr),
      m_hLabelSpeed(nullptr),
      m_hListVideos(nullptr),
      m_hVideoPanel(nullptr),
      m_hBtnPlay(nullptr),
      m_hBtnPause(nullptr),
      m_hBtnStop(nullptr),
      m_hBtnOpenFolder(nullptr),
      m_hBtnRefreshList(nullptr),
      m_hFontUI(nullptr),
      m_hFontBold(nullptr),
      m_isDownloading(false),
      m_downloadsDir("downloads") {
    m_downloader = std::make_unique<Downloader>();
    m_videoPlayer = std::make_unique<VideoPlayer>();

    // Garantir existência da pasta downloads
    fs::create_directories(m_downloadsDir);
}

MainWindow::~MainWindow() {
    if (m_isDownloading) {
        if (m_downloader) m_downloader->cancel();
        if (m_downloadThread.joinable()) m_downloadThread.join();
    }
    if (m_hFontUI) DeleteObject(m_hFontUI);
    if (m_hFontBold) DeleteObject(m_hFontBold);
}

bool MainWindow::create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    // Inicializar Controles Comuns (Progress Bar, etc.)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"VideoDownloaderMainWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClassW(&wc);

    // Fonte moderna Segoe UI
    m_hFontUI = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Video Downloader & Player (C++ / MinGW-w64)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 680,
        NULL, NULL, hInstance, this
    );

    if (!m_hWnd) return false;

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    refreshDownloadedList();

    return true;
}

int MainWindow::run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) return pThis->handleMessage(hWnd, uMsg, wParam, lParam);
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void MainWindow::createControls(HWND hWnd) {
    // Grupo 1: Entrada de URL e Ações de Download
    CreateWindowW(L"STATIC", L"URL do Vídeo:", WS_CHILD | WS_VISIBLE,
                  20, 15, 100, 20, hWnd, NULL, m_hInstance, NULL);

    m_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                125, 12, 530, 26, hWnd, (HMENU)IDC_EDIT_URL, m_hInstance, NULL);

    m_hBtnDownloadUrl = CreateWindowW(L"BUTTON", L"Baixar URL",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     665, 12, 100, 26, hWnd, (HMENU)IDC_BTN_DOWNLOAD_URL, m_hInstance, NULL);

    m_hBtnDownloadIni = CreateWindowW(L"BUTTON", L"Baixar de .INI...",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     775, 12, 125, 26, hWnd, (HMENU)IDC_BTN_DOWNLOAD_INI, m_hInstance, NULL);

    m_hBtnCancel = CreateWindowW(L"BUTTON", L"Cancelar",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                                905, 12, 80, 26, hWnd, (HMENU)IDC_BTN_CANCEL, m_hInstance, NULL);

    // Barra de Progresso e Status
    m_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                                   WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                   20, 48, 965, 20, hWnd, (HMENU)IDC_PROGRESS_BAR, m_hInstance, NULL);
    SendMessageW(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    m_hLabelStatus = CreateWindowW(L"STATIC", L"Pronto para baixar.",
                                  WS_CHILD | WS_VISIBLE,
                                  20, 75, 600, 20, hWnd, (HMENU)IDC_LABEL_STATUS, m_hInstance, NULL);

    m_hLabelSpeed = CreateWindowW(L"STATIC", L"",
                                 WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                 630, 75, 355, 20, hWnd, (HMENU)IDC_LABEL_SPEED, m_hInstance, NULL);

    // Coluna Esquerda: Lista de Vídeos Baixados
    CreateWindowW(L"STATIC", L"Vídeos Baixados na pasta 'downloads':", WS_CHILD | WS_VISIBLE,
                  20, 105, 300, 20, hWnd, NULL, m_hInstance, NULL);

    m_hListVideos = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                   WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL,
                                   20, 130, 280, 460, hWnd, (HMENU)IDC_LIST_VIDEOS, m_hInstance, NULL);

    m_hBtnRefreshList = CreateWindowW(L"BUTTON", L"Atualizar Lista",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     20, 600, 135, 28, hWnd, (HMENU)IDC_BTN_REFRESH, m_hInstance, NULL);

    m_hBtnOpenFolder = CreateWindowW(L"BUTTON", L"Abrir Pasta",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    165, 600, 135, 28, hWnd, (HMENU)IDC_BTN_OPEN_FOLDER, m_hInstance, NULL);

    // Coluna Direita: Player de Vídeo
    CreateWindowW(L"STATIC", L"Reprodutor Integrado:", WS_CHILD | WS_VISIBLE,
                  320, 105, 200, 20, hWnd, NULL, m_hInstance, NULL);

    // Painel de vídeo (área preta onde o DirectShow renderiza)
    m_hVideoPanel = CreateWindowExW(WS_EX_STATICEDGE, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                                   320, 130, 665, 460, hWnd, (HMENU)IDC_VIDEO_PANEL, m_hInstance, NULL);

    m_hBtnPlay = CreateWindowW(L"BUTTON", L"▶ Reproduzir",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              320, 600, 110, 28, hWnd, (HMENU)IDC_BTN_PLAY, m_hInstance, NULL);

    m_hBtnPause = CreateWindowW(L"BUTTON", L"⏸ Pausar",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               440, 600, 110, 28, hWnd, (HMENU)IDC_BTN_PAUSE, m_hInstance, NULL);

    m_hBtnStop = CreateWindowW(L"BUTTON", L"⏹ Parar",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              560, 600, 110, 28, hWnd, (HMENU)IDC_BTN_STOP, m_hInstance, NULL);

    // Aplicar fonte a todos os controles
    HWND controls[] = {
        m_hEditUrl, m_hBtnDownloadUrl, m_hBtnDownloadIni, m_hBtnCancel,
        m_hLabelStatus, m_hLabelSpeed, m_hListVideos,
        m_hBtnPlay, m_hBtnPause, m_hBtnStop, m_hBtnRefreshList, m_hBtnOpenFolder
    };
    for (HWND c : controls) {
        if (c) SendMessageW(c, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    }
}

void MainWindow::updateLayout(int width, int height) {
    if (!m_hEditUrl) return;

    int topBarY = 12;
    int topBarH = 26;
    int rightMargin = 20;

    int btnCancelW = 80;
    int btnIniW = 130;
    int btnUrlW = 95;
    int spacing = 8;

    int rightEdge = width - rightMargin;
    int cancelX = rightEdge - btnCancelW;
    int iniX = cancelX - spacing - btnIniW;
    int urlBtnX = iniX - spacing - btnUrlW;
    int editUrlW = urlBtnX - spacing - 125;
    if (editUrlW < 150) editUrlW = 150;

    MoveWindow(m_hEditUrl, 125, topBarY, editUrlW, topBarH, TRUE);
    MoveWindow(m_hBtnDownloadUrl, urlBtnX, topBarY, btnUrlW, topBarH, TRUE);
    MoveWindow(m_hBtnDownloadIni, iniX, topBarY, btnIniW, topBarH, TRUE);
    MoveWindow(m_hBtnCancel, cancelX, topBarY, btnCancelW, topBarH, TRUE);

    int progressW = width - 40;
    MoveWindow(m_hProgressBar, 20, 48, progressW, 20, TRUE);
    MoveWindow(m_hLabelStatus, 20, 75, (progressW * 6) / 10, 20, TRUE);
    MoveWindow(m_hLabelSpeed, 20 + (progressW * 6) / 10, 75, (progressW * 4) / 10, 20, TRUE);

    // Ajustar listbox e painel de vídeo
    int contentY = 130;
    int bottomControlsH = 40;
    int contentH = height - contentY - bottomControlsH - 20;
    if (contentH < 150) contentH = 150;

    int listW = 260;
    MoveWindow(m_hListVideos, 20, contentY, listW, contentH, TRUE);

    int videoX = 20 + listW + 15;
    int videoW = width - videoX - 20;
    if (videoW < 200) videoW = 200;
    MoveWindow(m_hVideoPanel, videoX, contentY, videoW, contentH, TRUE);

    int buttonY = contentY + contentH + 10;
    MoveWindow(m_hBtnRefreshList, 20, buttonY, 125, 28, TRUE);
    MoveWindow(m_hBtnOpenFolder, 155, buttonY, 125, 28, TRUE);

    MoveWindow(m_hBtnPlay, videoX, buttonY, 100, 28, TRUE);
    MoveWindow(m_hBtnPause, videoX + 110, buttonY, 100, 28, TRUE);
    MoveWindow(m_hBtnStop, videoX + 220, buttonY, 100, 28, TRUE);

    if (m_videoPlayer && m_videoPlayer->isLoaded()) {
        RECT rc;
        GetClientRect(m_hVideoPanel, &rc);
        m_videoPlayer->resize(0, 0, rc.right - rc.left, rc.bottom - rc.top);
    }
}

void MainWindow::refreshDownloadedList() {
    SendMessageW(m_hListVideos, LB_RESETCONTENT, 0, 0);
    m_videoFilePaths.clear();

    if (!fs::exists(m_downloadsDir)) {
        fs::create_directories(m_downloadsDir);
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_downloadsDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                // Extensões de vídeo suportadas
                if (ext == ".mp4" || ext == ".avi" || ext == ".wmv" || 
                    ext == ".mov" || ext == ".mkv" || ext == ".webm" || 
                    ext == ".mpg" || ext == ".mpeg" || ext == ".flv") {
                    
                    std::string filename = entry.path().filename().string();
                    m_videoFilePaths.push_back(entry.path().string());

                    // Converter para wstring para o ListBox
                    int len = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, NULL, 0);
                    std::wstring wFilename(len, 0);
                    MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, &wFilename[0], len);

                    SendMessageW(m_hListVideos, LB_ADDSTRING, 0, (LPARAM)wFilename.c_str());
                }
            }
        }
    } catch (...) {}
}

void MainWindow::playSelectedVideo() {
    int sel = static_cast<int>(SendMessageW(m_hListVideos, LB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(m_videoFilePaths.size())) {
        std::string videoPath = m_videoFilePaths[sel];
        if (m_videoPlayer->open(videoPath, m_hVideoPanel)) {
            m_videoPlayer->play();
        } else {
            MessageBoxW(m_hWnd, L"Não foi possível abrir o vídeo selecionado no player DirectShow.",
                        L"Aviso", MB_ICONWARNING);
        }
    }
}

void MainWindow::startDownloadUrl() {
    if (m_isDownloading) return;

    wchar_t urlBuf[2048] = {0};
    GetWindowTextW(m_hEditUrl, urlBuf, 2048);
    int len = WideCharToMultiByte(CP_UTF8, 0, urlBuf, -1, NULL, 0, NULL, NULL);
    std::string url(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, urlBuf, -1, &url[0], len, NULL, NULL);
    // Remover caractere nulo do final caso presente
    if (!url.empty() && url.back() == '\0') url.pop_back();

    if (url.empty()) {
        MessageBoxW(m_hWnd, L"Por favor, digite ou cole uma URL válida para download.",
                    L"URL Vazia", MB_ICONINFORMATION);
        return;
    }

    m_isDownloading = true;
    EnableWindow(m_hBtnDownloadUrl, FALSE);
    EnableWindow(m_hBtnDownloadIni, FALSE);
    EnableWindow(m_hBtnCancel, TRUE);

    SendMessageW(m_hProgressBar, PBM_SETPOS, 0, 0);
    SetWindowTextW(m_hLabelStatus, L"Conectando...");
    SetWindowTextW(m_hLabelSpeed, L"");

    if (m_downloadThread.joinable()) {
        m_downloadThread.join();
    }

    m_downloadThread = std::thread([this, url]() {
        std::string filename = Downloader::getFilenameFromUrl(url);
        std::string destPath = m_downloadsDir + "/" + filename;

        bool success = m_downloader->download(url, destPath,
            [this](const DownloadProgress& p) -> bool {
                auto* payload = new ProgressPayload{
                    p.percentage,
                    p.speedFormatted,
                    p.downloadedFormatted + " / " + p.totalFormatted
                };
                PostMessageW(m_hWnd, WM_APP_DOWNLOAD_PROGRESS, 0, (LPARAM)payload);
                return !m_downloader->isCancelled();
            },
            [this](const std::string& statusMsg) {
                auto* msgStr = new std::string(statusMsg);
                PostMessageW(m_hWnd, WM_APP_DOWNLOAD_LOG, 0, (LPARAM)msgStr);
            }
        );

        PostMessageW(m_hWnd, WM_APP_DOWNLOAD_FINISHED, success ? 1 : 0, 0);
    });
}

void MainWindow::startDownloadIni() {
    if (m_isDownloading) return;

    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"Arquivos INI (*.ini)\0*.ini\0Todos os Arquivos (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, NULL, 0, NULL, NULL);
        std::string iniPath(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &iniPath[0], len, NULL, NULL);
        if (!iniPath.empty() && iniPath.back() == '\0') iniPath.pop_back();

        IniParser parser;
        if (!parser.loadFromFile(iniPath)) {
            MessageBoxW(m_hWnd, L"Erro ao carregar o arquivo .ini especificado.", L"Erro", MB_ICONERROR);
            return;
        }

        std::vector<std::string> urls = parser.getUrls();
        if (urls.empty()) {
            MessageBoxW(m_hWnd, L"Nenhuma URL encontrada no arquivo .ini.", L"Aviso", MB_ICONINFORMATION);
            return;
        }

        m_isDownloading = true;
        EnableWindow(m_hBtnDownloadUrl, FALSE);
        EnableWindow(m_hBtnDownloadIni, FALSE);
        EnableWindow(m_hBtnCancel, TRUE);

        if (m_downloadThread.joinable()) {
            m_downloadThread.join();
        }

        m_downloadThread = std::thread([this, urls]() {
            int total = static_cast<int>(urls.size());
            int completed = 0;

            for (size_t i = 0; i < urls.size(); ++i) {
                if (m_downloader->isCancelled()) break;

                const std::string& url = urls[i];
                std::string filename = Downloader::getFilenameFromUrl(url);
                std::string destPath = m_downloadsDir + "/" + filename;

                std::ostringstream ss;
                ss << "[" << (i + 1) << "/" << total << "] Baixando: " << filename;
                auto* initialMsg = new std::string(ss.str());
                PostMessageW(m_hWnd, WM_APP_DOWNLOAD_LOG, 0, (LPARAM)initialMsg);

                bool ok = m_downloader->download(url, destPath,
                    [this, i, total](const DownloadProgress& p) -> bool {
                        std::ostringstream ssStatus;
                        ssStatus << "[" << (i + 1) << "/" << total << "] " << p.downloadedFormatted << " / " << p.totalFormatted;
                        auto* payload = new ProgressPayload{
                            p.percentage,
                            p.speedFormatted,
                            ssStatus.str()
                        };
                        PostMessageW(m_hWnd, WM_APP_DOWNLOAD_PROGRESS, 0, (LPARAM)payload);
                        return !m_downloader->isCancelled();
                    },
                    nullptr
                );

                if (ok) completed++;
            }

            PostMessageW(m_hWnd, WM_APP_DOWNLOAD_FINISHED, completed, total);
        });
    }
}

void MainWindow::cancelDownload() {
    if (m_isDownloading && m_downloader) {
        m_downloader->cancel();
        SetWindowTextW(m_hLabelStatus, L"Cancelando download...");
    }
}

void MainWindow::openContainingFolder() {
    std::wstring wideDir = L"downloads";
    ShellExecuteW(NULL, L"open", wideDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

LRESULT MainWindow::handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        createControls(hWnd);
        return 0;

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        updateLayout(width, height);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId) {
        case IDC_BTN_DOWNLOAD_URL:
            startDownloadUrl();
            break;

        case IDC_BTN_DOWNLOAD_INI:
            startDownloadIni();
            break;

        case IDC_BTN_CANCEL:
            cancelDownload();
            break;

        case IDC_BTN_PLAY:
            if (m_videoPlayer->isLoaded()) {
                m_videoPlayer->play();
            } else {
                playSelectedVideo();
            }
            break;

        case IDC_BTN_PAUSE:
            m_videoPlayer->pause();
            break;

        case IDC_BTN_STOP:
            m_videoPlayer->stop();
            break;

        case IDC_BTN_REFRESH:
            refreshDownloadedList();
            break;

        case IDC_BTN_OPEN_FOLDER:
            openContainingFolder();
            break;

        case IDC_LIST_VIDEOS:
            if (wmEvent == LBN_DBLCLK) {
                playSelectedVideo();
            }
            break;
        }
        return 0;
    }

    case WM_APP_DOWNLOAD_PROGRESS: {
        auto* payload = reinterpret_cast<ProgressPayload*>(lParam);
        if (payload) {
            SendMessageW(m_hProgressBar, PBM_SETPOS, static_cast<WPARAM>(payload->percentage), 0);

            std::wstring wProgress(payload->progressStr.begin(), payload->progressStr.end());
            std::wstring wSpeed(payload->speedStr.begin(), payload->speedStr.end());

            std::wostringstream ssStatus;
            ssStatus << L"Baixando: " << static_cast<int>(payload->percentage) << L"% (" << wProgress << L")";
            SetWindowTextW(m_hLabelStatus, ssStatus.str().c_str());
            SetWindowTextW(m_hLabelSpeed, (L"Velocidade: " + wSpeed).c_str());

            delete payload;
        }
        return 0;
    }

    case WM_APP_DOWNLOAD_LOG: {
        auto* msgStr = reinterpret_cast<std::string*>(lParam);
        if (msgStr) {
            std::wstring wMsg(msgStr->begin(), msgStr->end());
            SetWindowTextW(m_hLabelStatus, wMsg.c_str());
            delete msgStr;
        }
        return 0;
    }

    case WM_APP_DOWNLOAD_FINISHED: {
        m_isDownloading = false;
        EnableWindow(m_hBtnDownloadUrl, TRUE);
        EnableWindow(m_hBtnDownloadIni, TRUE);
        EnableWindow(m_hBtnCancel, FALSE);

        if (wParam > 0) {
            SetWindowTextW(m_hLabelStatus, L"Download(s) concluído(s) com sucesso!");
        } else {
            SetWindowTextW(m_hLabelStatus, L"Download finalizado ou interrompido.");
        }
        SetWindowTextW(m_hLabelSpeed, L"");
        SendMessageW(m_hProgressBar, PBM_SETPOS, 100, 0);

        refreshDownloadedList();
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
