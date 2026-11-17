#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <vector>
#include <atomic>

namespace xen {

struct TickInfo {
    std::uint64_t index = 0;
    double periodSeconds = 0;
    double seconds = 0;
};

class RtObject {
public:
    virtual ~RtObject() = default;
    virtual void tick(const TickInfo& tick) noexcept;
};

class RtBuilder {
public:
    void add(std::unique_ptr<RtObject> object);
    std::vector<std::unique_ptr<RtObject>> take();

private:
    std::vector<std::unique_ptr<RtObject>> objects_;
};

class RtSnapshot {
public:
    RtSnapshot(std::uint64_t generation, std::vector<std::unique_ptr<RtObject>> objects);

    std::uint64_t generation() const { return generation_; }
    void tick(const TickInfo& tick) noexcept;
    const std::vector<std::unique_ptr<RtObject>>& objects() const { return objects_; }

private:
    std::uint64_t generation_;
    std::vector<std::unique_ptr<RtObject>> objects_;
};

// Message thread publishes. The clock loads a raw pointer and acknowledges the
// generation it finished. Retired snapshots are freed on the message thread
// only after that acknowledgement, or after the clock has been joined.
class SnapshotStore {
public:
    void publish(std::unique_ptr<RtSnapshot> snapshot);
    RtSnapshot* loadForTick() const noexcept;
    void acknowledge(std::uint64_t generation) noexcept;
    void reclaim();
    void reclaimAfterJoin();
    RtSnapshot* current() const noexcept;

private:
    std::atomic<RtSnapshot*> published_{nullptr};
    std::atomic<std::uint64_t> acknowledged_{0};
    std::deque<std::unique_ptr<RtSnapshot>> retained_;
    std::uint64_t latestGeneration_ = 0;
};

}
