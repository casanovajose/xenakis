#pragma once

#include <cstdint>

namespace xen::rt {

struct RtBlock {
    std::uint64_t index = 0;
    double periodSeconds = 0;
    double seconds = 0;
};

struct TransportTime {
    double seconds = 0;
    double beats = 0;
    bool playing = false;
    bool jumped = false;
};

// Message-thread code reads the same numbers through the plugin that publishes
// this. `at` is the start of the block the clock just finished. `end` is the
// time after that tick. Paths use the pair as the span a cursor covered.
class TempoSource {
public:
    virtual ~TempoSource() = default;
    virtual TransportTime at(RtBlock block) const noexcept = 0;
    virtual TransportTime end() const noexcept = 0;
    virtual double gridUnitSeconds() const noexcept = 0;
    virtual double beatsToSeconds(double beats) const noexcept = 0;
    // Wall-clock Unix time at a transport position. The base returns 0 when
    // the source has no anchor.
    virtual double wallUnixAt(double transportSeconds) const noexcept {
        (void)transportSeconds;
        return 0;
    }
};

}
