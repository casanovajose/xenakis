#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xen {

class ComponentRegistry {
public:
    void add(std::string ownerId, std::string type);
    bool known(std::string_view ownerId, std::string_view type) const;
    const std::vector<std::pair<std::string, std::string>>& all() const { return types_; }

private:
    std::vector<std::pair<std::string, std::string>> types_;
};

}
