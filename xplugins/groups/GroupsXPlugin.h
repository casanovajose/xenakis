#pragma once

#include "xen/XPlugin.h"

namespace xen::groups {

class GroupsXPlugin final : public XPlugin {
public:
    XPluginInfo info() const override;
    void contribute(Registry& registry) override;
    void compile(const Document& document, RtBuilder& builder) override;
    void migrate(Json& data, int fromVersion) override;
};

// True when this entity is a muted group, or a parent group is muted.
// Other xPlugins do not call this. They read the Group / GroupMember JSON
// described in README.md. The function is the groups-owned check for tests.
bool effectiveMute(const Document& document, const EntityId& id);

}
