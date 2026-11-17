#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace xen {

// Single-producer, single-consumer queue. push and pop do not allocate.
template <typename T, std::size_t Capacity>
class RtChannel {
    static_assert(std::is_trivially_copyable_v<T>, "RtChannel carries plain values");
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity is a power of two");

public:
    bool push(const T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire))
            return false;
        slots_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        out = slots_[tail];
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t mask_ = Capacity - 1;
    T slots_[Capacity]{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}
