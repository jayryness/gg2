#include "Os.h"
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gg {

Os::Os() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    timerFrequency_ = freq.QuadPart;
}

int64_t Os::GetRawTimer() {
    LARGE_INTEGER timer;
    QueryPerformanceCounter(&timer);
    return timer.QuadPart;
}

void Os::Sleep(unsigned milliseconds) {
    ::Sleep(milliseconds);
}

void Os::GetMaxWindowSize(unsigned& widthOut, unsigned& heightOut) {
    widthOut = GetSystemMetrics(SM_CXSCREEN);
    heightOut = GetSystemMetrics(SM_CYSCREEN);

}

bool Os::GetClientSize(void const* windowHandle, unsigned& widthOut, unsigned& heightOut) {
    RECT rect = {};
    GetClientRect((HWND)windowHandle, &rect);
    unsigned widthOld = std::exchange(widthOut, rect.right - rect.left);
    unsigned heightOld = std::exchange(heightOut, rect.bottom - rect.top);
    return (widthOld != widthOut) | (heightOld != heightOut);
}

bool Os::IsDebuggerPresent() {
    return ::IsDebuggerPresent() == TRUE;
}

void Os::PrintDebug(char const * text) {
    OutputDebugStringA(text);
}

}
