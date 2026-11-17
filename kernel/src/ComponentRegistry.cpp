#include "xen/ComponentRegistry.h"

#include <stdexcept>

namespace xen {

void ComponentRegistry::add(std::string ownerId, std::string type) {
    if (ownerId.empty() || type.empty())
        throw std::runtime_error("component type is incomplete");
    if (known(ownerId, type))
        throw std::runtime_error("component type already registered " + ownerId + "/" + type);
    types_.emplace_back(std::move(ownerId), std::move(type));
}

bool ComponentRegistry::known(std::string_view ownerId, std::string_view type) const {
    for (const auto& item : types_) {
        if (item.first == ownerId && item.second == type)
            return true;
    }
    return false;
}

} // namespace xen
