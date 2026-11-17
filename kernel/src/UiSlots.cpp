#include "xen/UiSlots.h"

namespace xen {

void UiRegistry::add(UiContribution contribution) {
    items_.push_back(std::move(contribution));
}

} // namespace xen
