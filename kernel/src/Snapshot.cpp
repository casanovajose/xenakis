#include "xen/Snapshot.h"

namespace xen {

void RtObject::tick(const TickInfo&) noexcept {}

void RtBuilder::add(std::unique_ptr<RtObject> object) {
    if (object)
        objects_.push_back(std::move(object));
}

std::vector<std::unique_ptr<RtObject>> RtBuilder::take() { return std::move(objects_); }

RtSnapshot::RtSnapshot(std::uint64_t generation, std::vector<std::unique_ptr<RtObject>> objects)
    : generation_(generation), objects_(std::move(objects)) {}

void RtSnapshot::tick(const TickInfo& tick) noexcept {
    for (const auto& object : objects_) {
        if (object)
            object->tick(tick);
    }
}

void SnapshotStore::publish(std::unique_ptr<RtSnapshot> snapshot) {
    latestGeneration_ = snapshot->generation();
    RtSnapshot* raw = snapshot.get();
    retained_.push_back(std::move(snapshot));
    published_.store(raw, std::memory_order_release);
    reclaim();
}

RtSnapshot* SnapshotStore::loadForTick() const noexcept {
    return published_.load(std::memory_order_acquire);
}

void SnapshotStore::acknowledge(std::uint64_t generation) noexcept {
    acknowledged_.store(generation, std::memory_order_release);
}

RtSnapshot* SnapshotStore::current() const noexcept {
    return published_.load(std::memory_order_acquire);
}

void SnapshotStore::reclaim() {
    const auto acknowledged = acknowledged_.load(std::memory_order_acquire);
    while (retained_.size() > 1) {
        RtSnapshot* oldest = retained_.front().get();
        if (oldest == nullptr)
            break;
        if (acknowledged <= oldest->generation())
            break;
        if (acknowledged < latestGeneration_)
            break;
        if (published_.load(std::memory_order_acquire) == oldest)
            break;
        retained_.pop_front();
    }
}

void SnapshotStore::reclaimAfterJoin() {
    RtSnapshot* live = published_.load(std::memory_order_acquire);
    while (!retained_.empty() && retained_.front().get() != live)
        retained_.pop_front();
}

} // namespace xen
