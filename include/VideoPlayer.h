#define VIDEO_PLAYER_H
#include <windows.h>
#include <dshow.h>
#include <string>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool open(const std::string& filePath, HWND hWndVideoContainer);
    void play();
    void pause();
    void stop();
    void close();

    void resize(int x, int y, int width, int height);

    bool isPlaying() const;
    bool isLoaded() const;

    double getDuration() const; // em segundos
    double getCurrentPosition() const; // em segundos
    void seekTo(double seconds);

private:
    void cleanup();

    IGraphBuilder* m_pGraphBuilder;
    IMediaControl* m_pMediaControl;
    IMediaEventEx* m_pMediaEvent;
    IVideoWindow*  m_pVideoWindow;
    IMediaSeeking* m_pMediaSeeking;

    HWND m_hContainerWnd;
    bool m_isLoaded;
    bool m_isPlaying;
};
