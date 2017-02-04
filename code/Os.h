#pragma once
#ifndef GG_OS_H
#define GG_OS_H

#include <cstdint>

namespace gg {

class Os {

public:
    Os();

    int64_t getTimerFrequency() const {
        return timerFrequency_;
    }
    int64_t asMicroseconds(int64_t ticks) const {
        int64_t high = (ticks >> 32) * 1000000 / timerFrequency_;
        int64_t low = (ticks & 0xffffffff) * 1000000 / timerFrequency_;
        return (high << 32) + low;
    }
    float asFloatMilliseconds(int64_t ticks) const {
        enum { cUnitDivisor = 1024 };
        int64_t high = (ticks >> 32) * (1000*cUnitDivisor) / timerFrequency_;
        int64_t low = (ticks & 0xffffffff) * (1000*cUnitDivisor) / timerFrequency_;
        return (float)((high << 32) + low) * (1.f/cUnitDivisor);
    }
    static int64_t GetRawTimer();

    static void Sleep(unsigned milliseconds);

    static void GetMaxWindowSize(unsigned& widthOut, unsigned& heightOut);
    static bool GetClientSize(void const* windowHandle, unsigned& widthOut, unsigned& heightOut);

    static bool IsDebuggerPresent();
    static void PrintDebug(char const* text);

private:
    uint64_t timerFrequency_;
};

}

#endif
