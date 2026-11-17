#pragma once

#include "xen/Json.h"
#include "xen/Registry.h"
#include "xen/Snapshot.h"

#include <string>
#include <vector>

namespace xen {

struct Dependency {
    std::string id;
};

// `required` is the essay's hard "requires" list. `requires` is a C++20 keyword.
struct XPluginInfo {
    std::string id;
    std::string name;
    int version = 1;
    std::vector<Dependency> required;
    std::vector<Dependency> enhances;
};

class XPlugin {
public:
    virtual ~XPlugin() = default;
    virtual XPluginInfo info() const = 0;
    virtual void contribute(Registry& registry) = 0;
    virtual void activate(Document&) {}
    virtual void deactivate(Document&) {}
    virtual void compile(const Document&, RtBuilder&) {}
    virtual void migrate(Json&, int) {}
};

}
