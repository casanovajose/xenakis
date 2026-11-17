#pragma once

#include "rt/PathsNode.h"
#include "xen/XPlugin.h"
#include "xen/contracts/PositionTap.h"

namespace xen::paths {

class PathsXPlugin final : public XPlugin, public xen::rt::PositionTap {
public:
    XPluginInfo info() const override;
    void contribute(Registry& registry) override;
    void activate(Document& document) override;
    void deactivate(Document& document) override;
    void compile(const Document& document, RtBuilder& builder) override;
    void pump(EventBus& events) override;
    void migrate(Json& data, int fromVersion) override;
    bool pop(xen::rt::CursorSample& sample) noexcept override;

private:
    rt::PlaySlot slots_[128]{};
    xen::RtChannel<rt::Notice, 2048> notices_;
    xen::RtChannel<xen::rt::CursorSample, 256> positions_;
};

}
