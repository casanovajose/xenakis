#pragma once

#include <string>
#include <vector>

namespace xen {

// Slot ids for the shell frame. The frame itself is M5; plugins only name a slot.
enum class UiSlot {
    ToolbarTools,
    ToolbarTransport,
    Layers,
    Canvas,
    Inspector,
    Bottom
};

struct UiContribution {
    UiSlot slot = UiSlot::Inspector;
    std::string ownerId;
    std::string widgetId;
};

class UiRegistry {
public:
    void add(UiContribution contribution);
    const std::vector<UiContribution>& all() const { return items_; }

private:
    std::vector<UiContribution> items_;
};

class XPluginUi {
public:
    virtual ~XPluginUi() = default;
    virtual void contributeUi(UiRegistry& ui) = 0;
};

}
