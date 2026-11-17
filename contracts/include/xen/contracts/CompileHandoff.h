#pragma once

#include "xen/contracts/PositionTap.h"
#include "xen/contracts/TempoSource.h"

namespace xen::contracts {

// Filled during compile on the message thread. The transport node publishes
// itself; later xPlugins in the same compile store the pointer. The pointer
// stays valid for the snapshot that contains both objects.
inline rt::TempoSource*& tempoSlot() noexcept {
    static rt::TempoSource* pointer = nullptr;
    return pointer;
}

inline void publishTempoSource(rt::TempoSource* source) noexcept {
    tempoSlot() = source;
}

inline rt::TempoSource* tempoSource() noexcept {
    return tempoSlot();
}

inline rt::PositionTap*& positionSlot() noexcept {
    static rt::PositionTap* pointer = nullptr;
    return pointer;
}

inline void publishPositionTap(rt::PositionTap* tap) noexcept {
    positionSlot() = tap;
}

inline rt::PositionTap* positionTap() noexcept {
    return positionSlot();
}

}
