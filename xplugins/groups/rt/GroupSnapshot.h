#pragma once

#include "xen/Snapshot.h"

#include <string>
#include <vector>

namespace xen::groups::rt {

struct GroupItem {
    std::string id;
    std::string name;
    bool visible = true;
    bool mute = false;
    std::vector<std::string> children;
};

// Filled on the message thread inside compile. tick only counts.
class GroupSnapshot final : public xen::RtObject {
public:
    explicit GroupSnapshot(std::vector<GroupItem> items) : items_(std::move(items)) {}

    void tick(const xen::TickInfo&) noexcept override { ++ticks_; }

    std::uint64_t ticks() const noexcept { return ticks_; }
    const std::vector<GroupItem>& items() const noexcept { return items_; }

private:
    std::vector<GroupItem> items_;
    std::uint64_t ticks_ = 0;
};

}
