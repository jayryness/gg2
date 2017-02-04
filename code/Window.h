#pragma once
#ifndef GG_WINDOW_H
#define GG_WINDOW_H

namespace gg {

class Window {
public:
    static void ShowConsole(bool visible);

    Window(char const* title);
    ~Window();

    void waitForClose() const;
    bool isClosing() const;

    void* hwnd() const {
        return hwnd_;
    }

    bool isMaximized() const;

private:
    friend struct Callbacks;
    long long windowProc(unsigned uMsg, unsigned long long wParam, long long lParam);

    void* hwnd_ = nullptr;
    void* hthread_ = nullptr;
    int savedX_ = 0;
    int savedY_ = 0;
    unsigned savedWidth_ = 0;
    unsigned savedHeight_ = 0;
};

}

#endif
