#include "xen/Registry.h"

#include <stdexcept>

namespace xen {

Registry::Registry(PropertyRegistry& properties, CommandRegistry& commands, ComponentRegistry& components)
    : properties_(properties), commands_(commands), components_(components) {}

void Registry::addEvent(std::string ownerId, std::string name) {
    if (ownerId.empty() || name.empty())
        throw std::runtime_error("event name is incomplete");
    events_.emplace_back(std::move(ownerId), std::move(name));
}

void Registry::addAction(std::string ownerId, std::string name) {
    if (ownerId.empty() || name.empty())
        throw std::runtime_error("action name is incomplete");
    actions_.emplace_back(std::move(ownerId), std::move(name));
}

} // namespace xen
