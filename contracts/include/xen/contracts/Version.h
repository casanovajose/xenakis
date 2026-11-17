#pragma once

namespace xen::contracts {

// Bump when a shared contract header changes in a way callers must notice.
// TempoSource, PositionSource, and the other feature contracts arrive with
// the xPlugin that provides them (TempoSource with xen.transport, M2).
inline constexpr int kVersion = 1;

}
