#include "Window.h"
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace gg {

struct Bootstrap {
    Window* window_;
    char const* title_;
    volatile HWND hwnd_;
};

struct Callbacks {
    static DWORD CALLBACK ThreadProc(LPVOID lpParameter) {
        HWND hwnd = nullptr;
        {
            Bootstrap* bootstrap = (Bootstrap*)lpParameter;

            WNDCLASSEX wc;
            wc.cbSize        = sizeof(WNDCLASSEX);
            wc.style         = CS_DBLCLKS;
            wc.lpfnWndProc   = WindowProc;
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = 0;
            wc.hInstance     = GetModuleHandle(nullptr);
            wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
            wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
            wc.lpszMenuName  = nullptr;
            wc.lpszClassName = L"gg";
            wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

            ATOM registeredClass = RegisterClassEx(&wc);

            hwnd = CreateWindowExA(
                0,
                (LPCSTR)registeredClass,
                bootstrap->title_,
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 960, 540,
                NULL, NULL, wc.hInstance, NULL);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)bootstrap->window_);

            InterlockedExchangePointer((PVOID*)&std::exchange(bootstrap, nullptr)->hwnd_, hwnd);
        }

        ShowWindow(hwnd, SW_SHOW);

        MSG msg;
        while (GetMessage(&msg, hwnd, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return 0;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Window* window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        return window ? window->windowProc(uMsg, wParam, lParam) : DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
};

void Window::ShowConsole(bool visible) {
    ShowWindow(GetConsoleWindow(), visible ? SW_SHOW : SW_HIDE);
}

Window::Window(char const* title) {
    Bootstrap bootstrap = {this, title, nullptr};
    hthread_ = CreateThread(
        nullptr,
        0,
        Callbacks::ThreadProc,
        &bootstrap,
        0,
        nullptr);
    while(bootstrap.hwnd_ == nullptr)
        ;
    hwnd_ = bootstrap.hwnd_;
}

Window::~Window() {
    PostMessage((HWND)hwnd_, WM_QUIT, 0, 0);
    waitForClose();
}

bool Window::isMaximized() const {
    WINDOWPLACEMENT placement = {};
    GetWindowPlacement((HWND)hwnd_, &placement);
    return placement.showCmd == SW_SHOWMAXIMIZED;
}

void Window::waitForClose() const {
    WaitForSingleObject((HANDLE)hthread_, INFINITE);
}

bool Window::isClosing() const {
    RECT rect = {};
    return !GetWindowRect((HWND)hwnd_, &rect);
}

long long Window::windowProc(unsigned msg, unsigned long long wParam, long long lParam) {
    auto hwnd = (HWND)hwnd_;
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SYSCOMMAND:
            if (wParam == SC_MAXIMIZE) {
                RECT wr;
                GetWindowRect(hwnd, &wr);
                savedX_ = wr.left;
                savedY_ = wr.top;
                savedWidth_ = wr.right - wr.left;
                savedHeight_ = wr.bottom - wr.top;
                SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
            } else if (wParam == SC_RESTORE) {
                MoveWindow(hwnd, savedX_, savedY_, savedWidth_, savedHeight_, false);
                SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
            }
            return DefWindowProc(hwnd, WM_SYSCOMMAND, wParam, lParam);

        case WM_LBUTTONDBLCLK:
            if (isMaximized()) {
                PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
            } else {
                PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
            }
            break;

        default:
            return DefWindowProc (hwnd, msg, wParam, lParam);
   }
   return 0;
}

}
