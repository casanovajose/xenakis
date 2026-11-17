#pragma once

#include "xen/Snapshot.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace xen {

class ControlClock {
public:
    struct Target {
        virtual ~Target() = default;
        virtual void onTick(const TickInfo& tick) noexcept = 0;
    };

    explicit ControlClock(Target& target);
    ~ControlClock();

    ControlClock(const ControlClock&) = delete;
    ControlClock& operator=(const ControlClock&) = delete;

    void start(double periodSeconds);
    void stop();
    bool running() const;
    std::uint64_t ticks() const;

private:
    void threadMain();

    Target& target_;
    std::mutex life_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    double period_ = 0.005;
    std::atomic<std::uint64_t> ticks_{0};
};

const char* preciseSleepBackend();
void preciseSleepUntil(std::chrono::steady_clock::time_point deadline);

}
