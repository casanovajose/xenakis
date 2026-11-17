#pragma once

namespace xen::contracts {

inline double beatsToSeconds(double beats, double bpm) noexcept {
    return bpm > 0.0 ? beats * 60.0 / bpm : 0.0;
}

inline double secondsToBeats(double seconds, double bpm) noexcept {
    return bpm > 0.0 ? seconds * bpm / 60.0 : 0.0;
}

// One grid unit. BPM mode counts `gridUnit` in beats. Absolute mode counts it
// in `absoluteUnitSeconds` (1 for seconds, 0.001 for ms, 60 for minutes).
inline double gridUnitToSeconds(bool bpmMode, double gridUnit, double bpm, double absoluteUnitSeconds) noexcept {
    if (!(gridUnit > 0.0))
        return 0.0;
    if (!bpmMode)
        return gridUnit * (absoluteUnitSeconds > 0.0 ? absoluteUnitSeconds : 1.0);
    return beatsToSeconds(gridUnit, bpm);
}

inline double absoluteUnitSeconds(const char* unit) noexcept {
    if (unit == nullptr)
        return 1.0;
    if (unit[0] == 'm' && unit[1] == 's' && unit[2] == '\0')
        return 0.001;
    if (unit[0] == 'm' && unit[1] == 'i' && unit[2] == 'n' && unit[3] == '\0')
        return 60.0;
    return 1.0;
}

}
