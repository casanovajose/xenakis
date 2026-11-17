#pragma once

#include "xen/Snapshot.h"

namespace xen::__NS__::rt {

struct Nop final : xen::RtObject {
    void tick(const xen::TickInfo&) noexcept override {}
};

}
