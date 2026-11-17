#include "xen/ControlClock.h"

#ifndef _WIN32
#error "PreciseSleep_win.cpp is the Windows clock backend"
#endif

#include <windows.h>

namespace xen {
namespace {

struct TimerSlot {
    HANDLE handle = nullptr;
    TimerSlot() {
        handle = CreateWaitableTimerExW(
            nullptr,
            nullptr,
            0x00000002, // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
            TIMER_MODIFY_STATE | SYNCHRONIZE);
        if (handle == nullptr)
            handle = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    }
    ~TimerSlot() {
        if (handle != nullptr)
            CloseHandle(handle);
    }
};

struct TimerResolution {
    TimerResolution() { timeBeginPeriod(1); }
    ~TimerResolution() { timeEndPeriod(1); }
};

TimerSlot& timer() {
    static thread_local TimerSlot slot;
    return slot;
}

void keepResolution() {
    static TimerResolution resolution;
    (void)resolution;
}

void waitCoarse(std::chrono::nanoseconds amount) {
    if (amount.count() <= 0)
        return;
    keepResolution();
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(amount.count() / 100);
    if (due.QuadPart == 0)
        due.QuadPart = -1;
    HANDLE handle = timer().handle;
    if (handle != nullptr && SetWaitableTimer(handle, &due, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(handle, INFINITE);
        return;
    }
    Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(amount).count()));
}

} // namespace

const char* preciseSleepBackend() {
    return "CreateWaitableTimerEx high-resolution, timeBeginPeriod(1), short spin";
}

void preciseSleepUntil(std::chrono::steady_clock::time_point deadline) {
    using namespace std::chrono;
    keepResolution();
    for (;;) {
        const auto now = steady_clock::now();
        if (now >= deadline)
            return;
        const auto remain = deadline - now;
        if (remain > microseconds(1200)) {
            waitCoarse(duration_cast<nanoseconds>(remain - microseconds(500)));
            continue;
        }
        while (steady_clock::now() < deadline)
            YieldProcessor();
        return;
    }
}

} // namespace xen
