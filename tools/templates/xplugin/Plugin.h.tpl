#pragma once

#include "xen/XPlugin.h"

namespace xen::__NS__ {

class __CLASS__ final : public xen::XPlugin {
public:
    xen::XPluginInfo info() const override;
    void contribute(xen::Registry& registry) override;
    void compile(const xen::Document& document, xen::RtBuilder& builder) override;
};

}
