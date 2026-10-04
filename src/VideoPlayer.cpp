#include "VideoPlayer.h"
#include <iostream>

VideoPlayer::VideoPlayer() 
    : m_pGraphBuilder(nullptr),
      m_pMediaControl(nullptr),
      m_pMediaEvent(nullptr),
      m_pVideoWindow(nullptr),
      m_pMediaSeeking(nullptr),
      m_hContainerWnd(nullptr),
      m_isLoaded(false),
      m_isPlaying(false) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
}

VideoPlayer::~VideoPlayer() {
    cleanup();
    CoUninitialize();
}

void VideoPlayer::cleanup() {
    stop();

    if (m_pVideoWindow) {
        m_pVideoWindow->put_Visible(OAFALSE);
        m_pVideoWindow->put_Owner((OAHWND)0);
        m_pVideoWindow->Release();
        m_pVideoWindow = nullptr;
    }
    if (m_pMediaSeeking) {
        m_pMediaSeeking->Release();
        m_pMediaSeeking = nullptr;
    }
    if (m_pMediaEvent) {
        m_pMediaEvent->Release();
        m_pMediaEvent = nullptr;
    }
    if (m_pMediaControl) {
        m_pMediaControl->Release();
        m_pMediaControl = nullptr;
    }
    if (m_pGraphBuilder) {
        m_pGraphBuilder->Release();
        m_pGraphBuilder = nullptr;
    }

    m_isLoaded = false;
    m_isPlaying = false;
}

bool VideoPlayer::open(const std::string& filePath, HWND hWndVideoContainer) {
    cleanup();

    m_hContainerWnd = hWndVideoContainer;

    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                                 IID_IGraphBuilder, (void**)&m_pGraphBuilder);
    if (FAILED(hr)) return false;

    hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&m_pMediaControl);
    if (FAILED(hr)) { cleanup(); return false; }

    hr = m_pGraphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&m_pMediaEvent);
    if (FAILED(hr)) { cleanup(); return false; }

    hr = m_pGraphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&m_pMediaSeeking);
    if (FAILED(hr)) { cleanup(); return false; }

    // Converter std::string para LPCWSTR
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
    std::wstring widePath(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &widePath[0], wideLen);

    hr = m_pGraphBuilder->RenderFile(widePath.c_str(), NULL);
    if (FAILED(hr)) {
        // Tentar com codepage ANSI local caso UTF8 falhe
        int ansiLen = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, NULL, 0);
        std::wstring ansiWide(ansiLen, 0);
        MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, &ansiWide[0], ansiLen);
        hr = m_pGraphBuilder->RenderFile(ansiWide.c_str(), NULL);
        if (FAILED(hr)) {
            cleanup();
            return false;
        }
    }

    // Configura a janela de exibição do vídeo (IVideoWindow)
    hr = m_pGraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&m_pVideoWindow);
    if (SUCCEEDED(hr) && m_pVideoWindow && m_hContainerWnd) {
        m_pVideoWindow->put_Owner(reinterpret_cast<OAHWND>(m_hContainerWnd));
        m_pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

        RECT rc;
        GetClientRect(m_hContainerWnd, &rc);
        m_pVideoWindow->SetWindowPosition(0, 0, rc.right - rc.left, rc.bottom - rc.top);
        m_pVideoWindow->put_Visible(OATRUE);
    }

    m_isLoaded = true;
    return true;
}

void VideoPlayer::play() {
    if (m_pMediaControl && m_isLoaded) {
        HRESULT hr = m_pMediaControl->Run();
        if (SUCCEEDED(hr)) m_isPlaying = true;
    }
}

void VideoPlayer::pause() {
    if (m_pMediaControl && m_isLoaded) {
        m_pMediaControl->Pause();
        m_isPlaying = false;
    }
}

void VideoPlayer::stop() {
    if (m_pMediaControl && m_isLoaded) {
        m_pMediaControl->Stop();
        if (m_pMediaSeeking) {
            LONGLONG pos = 0;
            m_pMediaSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
                                         NULL, AM_SEEKING_NoPositioning);
        }
        m_isPlaying = false;
    }
}

void VideoPlayer::close() {
    cleanup();
}

void VideoPlayer::resize(int x, int y, int width, int height) {
    if (m_pVideoWindow) m_pVideoWindow->SetWindowPosition(x, y, width, height);
}

bool VideoPlayer::isPlaying() const {
    return m_isPlaying;
}

bool VideoPlayer::isLoaded() const {
    return m_isLoaded;
}

double VideoPlayer::getDuration() const {
    if (!m_pMediaSeeking) return 0.0;
    LONGLONG duration = 0;
    if (SUCCEEDED(m_pMediaSeeking->GetDuration(&duration))) {
        // TIME_FORMAT_MEDIA_TIME é 100-nanosegundos (10^7 por segundo)
        return static_cast<double>(duration) / 10000000.0;
    }
    return 0.0;
}

double VideoPlayer::getCurrentPosition() const {
    if (!m_pMediaSeeking) return 0.0;
    LONGLONG curPos = 0;
    if (SUCCEEDED(m_pMediaSeeking->GetCurrentPosition(&curPos))) {
        return static_cast<double>(curPos) / 10000000.0;
    }
    return 0.0;
}

void VideoPlayer::seekTo(double seconds) {
    if (m_pMediaSeeking) {
        LONGLONG pos = static_cast<LONGLONG>(seconds * 10000000.0);
        m_pMediaSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
                                     NULL, AM_SEEKING_NoPositioning);
    }
}
