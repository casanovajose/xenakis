#include "__CLASS__.h"
#include "rt/Nop.h"

#include <memory>
#include <optional>

namespace xen::__NS__ {
namespace {

xen::PropertyDesc enabledProperty() {
    return xen::PropertyDesc{
        "__ID__.enabled",
        "__ID__",
        "Settings",
        "enabled",
        xen::ValueType::Bool,
        false,
        std::nullopt,
        std::nullopt,
        "enabled",
    };
}

} // namespace

xen::XPluginInfo __CLASS__::info() const {
    return xen::XPluginInfo{"__ID__", "__NAME__", 1, {}, {}};
}

void __CLASS__::contribute(xen::Registry& registry) {
    registry.components().add("__ID__", "Settings");
    registry.properties().add(enabledProperty());
    registry.addEvent("__ID__", "__FOLDER__.ready");
}

void __CLASS__::compile(const xen::Document&, xen::RtBuilder& builder) {
    builder.add(std::make_unique<rt::Nop>());
}

} // namespace xen::__NS__
