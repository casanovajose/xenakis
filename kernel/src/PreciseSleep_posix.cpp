#include "xen/ControlClock.h"

#ifdef _WIN32
#error "PreciseSleep_posix.cpp is not the Windows clock backend"
#endif

#include <time.h>

namespace xen {
namespace {

void waitCoarse(std::chrono::nanoseconds amount) {
    if (amount.count() <= 0)
        return;
    timespec ts;
    ts.tv_sec = static_cast<time_t>(amount.count() / 1000000000LL);
    ts.tv_nsec = static_cast<long>(amount.count() % 1000000000LL);
#if defined(CLOCK_MONOTONIC)
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
#else
    nanosleep(&ts, nullptr);
#endif
}

} // namespace

const char* preciseSleepBackend() {
    return "clock_nanosleep plus a short spin";
}

void preciseSleepUntil(std::chrono::steady_clock::time_point deadline) {
    using namespace std::chrono;
    for (;;) {
        const auto now = steady_clock::now();
        if (now >= deadline)
            return;
        const auto remain = deadline - now;
        if (remain > microseconds(400)) {
            waitCoarse(duration_cast<nanoseconds>(remain - microseconds(200)));
            continue;
        }
        while (steady_clock::now() < deadline) {
        }
        return;
    }
}

} // namespace xen
