#include "__CLASS__.h"
#include "xen/UiSlots.h"

#include <memory>

namespace xen::__NS__ {

class __CLASS__Ui final : public xen::XPluginUi {
public:
    void contributeUi(xen::UiRegistry& ui) override {
        ui.add(xen::UiContribution{xen::UiSlot::Inspector, "__ID__", "enabled"});
    }
};

std::unique_ptr<xen::XPluginUi> create__CLASS__Ui() {
    return std::make_unique<__CLASS__Ui>();
}

} // namespace xen::__NS__
