#pragma once

#include "xen/ComponentRegistry.h"
#include "xen/PropertyRegistry.h"

#include <string>
#include <utility>
#include <vector>

namespace xen {

class Registry {
public:
    Registry(PropertyRegistry& properties, CommandRegistry& commands, ComponentRegistry& components);

    PropertyRegistry& properties() { return properties_; }
    CommandRegistry& commands() { return commands_; }
    ComponentRegistry& components() { return components_; }

    void addEvent(std::string ownerId, std::string name);
    void addAction(std::string ownerId, std::string name);

    const std::vector<std::pair<std::string, std::string>>& events() const { return events_; }
    const std::vector<std::pair<std::string, std::string>>& actions() const { return actions_; }

private:
    PropertyRegistry& properties_;
    CommandRegistry& commands_;
    ComponentRegistry& components_;
    std::vector<std::pair<std::string, std::string>> events_;
    std::vector<std::pair<std::string, std::string>> actions_;
};

}
