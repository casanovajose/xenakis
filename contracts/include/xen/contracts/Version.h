#pragma once

namespace xen::contracts {

// Bump when a shared contract header changes in a way callers must notice.
// TempoSource arrived with xen.transport (M2). PositionTap arrives with paths.
inline constexpr int kVersion = 2;

}
