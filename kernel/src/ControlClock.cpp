#include "xen/ControlClock.h"

namespace xen {

ControlClock::ControlClock(Target& target) : target_(target) {}

ControlClock::~ControlClock() { stop(); }

void ControlClock::start(double periodSeconds) {
    if (periodSeconds <= 0)
        periodSeconds = 0.005;
    std::lock_guard<std::mutex> lock(life_);
    if (thread_.joinable())
        return;
    period_ = periodSeconds;
    ticks_.store(0, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { threadMain(); });
}

void ControlClock::stop() {
    std::lock_guard<std::mutex> lock(life_);
    running_.store(false, std::memory_order_release);
    if (thread_.joinable())
        thread_.join();
}

bool ControlClock::running() const {
    return running_.load(std::memory_order_acquire);
}

std::uint64_t ControlClock::ticks() const {
    return ticks_.load(std::memory_order_acquire);
}

void ControlClock::threadMain() {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(period_));
    const auto start = clock::now();
    auto next = start + period;
    std::uint64_t index = 0;
    while (running_.load(std::memory_order_acquire)) {
        preciseSleepUntil(next);
        if (!running_.load(std::memory_order_acquire))
            break;
        const auto now = clock::now();
        TickInfo info;
        info.index = index++;
        info.periodSeconds = period_;
        info.seconds = std::chrono::duration<double>(now - start).count();
        target_.onTick(info);
        ticks_.store(index, std::memory_order_release);
        next += period;
        if (clock::now() > next + period)
            next = clock::now() + period;
    }
}

} // namespace xen
