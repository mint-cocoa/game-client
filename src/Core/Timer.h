#pragma once
#include <windows.h>

class Timer {
public:
    void Init() {
        QueryPerformanceFrequency(&freq_);
        QueryPerformanceCounter(&last_);
        start_ = last_;
    }

    float Tick() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last_.QuadPart) / freq_.QuadPart;
        last_ = now;
        if (dt > 0.1f) dt = 0.1f;
        return dt;
    }

    float GetTotalTime() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (float)(now.QuadPart - start_.QuadPart) / freq_.QuadPart;
    }

private:
    LARGE_INTEGER freq_{};
    LARGE_INTEGER last_{};
    LARGE_INTEGER start_{};
};
