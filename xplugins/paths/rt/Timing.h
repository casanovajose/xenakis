#pragma once

#include "rt/Geometry.h"

#include <algorithm>
#include <cmath>

namespace xen::paths::rt {

struct Hit {
    int triggerIndex = -1;
    int dir = 0;
    int pass = 0;
    double u = 0;
    double time = 0;
    bool passEnded = false;
    bool started = false;
    bool stopped = false;
};

struct CursorMath {
    double uIn = 0;
    double uOut = 1;
    double start = 0;
    double secondsPerU = 0;
    double delay = 0;
    int pattern[8]{};
    int patternCount = 1;
    bool once = false;

    static CursorMath make(double pathLength, double gridUnitSeconds, double speed, double speedFactor, double delay,
                           double startU, double loopIn, double loopOut, bool gridMode, double absoluteSeconds,
                           const int* pattern, int count, bool onceFlag) noexcept {
        CursorMath math;
        math.uIn = std::clamp(loopIn, 0.0, 1.0);
        math.uOut = std::clamp(loopOut, 0.0, 1.0);
        if (math.uOut < math.uIn)
            std::swap(math.uIn, math.uOut);
        math.start = std::clamp(startU, math.uIn, math.uOut);
        math.delay = delay;
        math.once = onceFlag;
        math.patternCount = std::clamp(count, 0, 8);
        for (int i = 0; i < math.patternCount; ++i)
            math.pattern[i] = pattern[i] >= 0 ? 1 : -1;
        if (math.patternCount == 0) {
            math.pattern[0] = 1;
            math.patternCount = 1;
        }
        const double scale = speed * speedFactor;
        const double span = math.uOut - math.uIn;
        if (!(scale > 0.0) || !(span > 0.0)) {
            math.secondsPerU = 0;
            return math;
        }
        if (gridMode)
            math.secondsPerU = pathLength * gridUnitSeconds / scale;
        else
            math.secondsPerU = absoluteSeconds / span;
        return math;
    }

    double passDuration() const noexcept { return (uOut - uIn) * secondsPerU; }

    void endpoints(int dir, bool first, double& u0, double& u1) const noexcept {
        if (dir >= 0) {
            u0 = first ? start : uIn;
            u1 = uOut;
        } else {
            u0 = first ? start : uOut;
            u1 = uIn;
        }
    }

    double stepDuration(int dir, bool first) const noexcept {
        double u0 = 0;
        double u1 = 0;
        endpoints(dir, first, u0, u1);
        return std::abs(u1 - u0) * secondsPerU;
    }

    struct Place {
        double t0 = 0;
        double t1 = 0;
        double u = 0;
        double u0 = 0;
        double u1 = 0;
        int dir = 0;
        int pass = 0;
        bool moving = false;
        bool finished = false;
    };

    Place placeAt(double cursorTime) const noexcept {
        Place place;
        place.u = start;
        place.u0 = start;
        place.u1 = start;
        if (!(secondsPerU > 0.0) || cursorTime < 0.0 || patternCount <= 0)
            return place;

        const double firstCycle = cycleDuration(true);
        const double laterCycle = cycleDuration(false);
        double local = cursorTime;
        double cycleOrigin = 0;
        int passBase = 0;
        bool firstCycleNow = true;
        if (!once) {
            if (local >= firstCycle && laterCycle > 0.0) {
                const double after = local - firstCycle;
                const int cycles = static_cast<int>(after / laterCycle);
                cycleOrigin = firstCycle + static_cast<double>(cycles) * laterCycle;
                local = cursorTime - cycleOrigin;
                passBase = patternCount * (cycles + 1);
                firstCycleNow = false;
            }
        } else if (!(local < firstCycle)) {
            double u0 = 0;
            double u1 = 0;
            endpoints(pattern[patternCount - 1], patternCount == 1, u0, u1);
            place.u = u1;
            place.u0 = u1;
            place.u1 = u1;
            place.pass = patternCount;
            place.finished = true;
            return place;
        }

        double cursor = 0;
        for (int i = 0; i < patternCount; ++i) {
            const bool first = firstCycleNow && i == 0;
            const double duration = stepDuration(pattern[i], first);
            const bool last = i + 1 == patternCount;
            if (local < cursor + duration || (last && local <= cursor + duration)) {
                double u0 = 0;
                double u1 = 0;
                endpoints(pattern[i], first, u0, u1);
                const double into = duration > 0.0 ? std::clamp(local - cursor, 0.0, duration) : 0.0;
                const double fraction = duration > 0.0 ? into / duration : 0.0;
                place.t0 = cycleOrigin + cursor;
                place.t1 = place.t0 + duration;
                place.u0 = u0;
                place.u1 = u1;
                place.u = u0 + (u1 - u0) * fraction;
                place.dir = pattern[i];
                place.pass = passBase + i + 1;
                place.moving = duration > 0.0 && into < duration;
                return place;
            }
            cursor += duration;
        }
        place.finished = once;
        return place;
    }

    template <typename Trigger>
    int crossings(double transport0, double transport1, const Trigger* triggers, int triggerCount, Hit* out,
                  int capacity) const noexcept {
        if (out == nullptr || capacity <= 0 || !(transport1 > transport0))
            return 0;
        const double t0 = transport0 - delay;
        const double t1 = transport1 - delay;
        if (!(secondsPerU > 0.0) || !(t1 > t0))
            return 0;
        int count = 0;
        double t = t0;
        bool emittedStart = false;
        for (int guard = 0; guard < 2048 && t < t1 && count < capacity; ++guard) {
            const Place place = placeAt(std::max(t, 0.0));
            if (t < 0.0 && t1 > 0.0 && !emittedStart && count < capacity) {
                out[count++] = Hit{-1, 0, 0, start, delay, false, true, false};
                emittedStart = true;
                t = 0.0;
                continue;
            }
            if (!place.moving)
                break;
            const double segEnd = place.t1 < t1 ? place.t1 : t1;
            if (!emittedStart && place.pass == 1 && t <= place.t0 && segEnd > place.t0 && count < capacity) {
                out[count++] = Hit{-1, place.dir, 1, place.u0, place.t0 + delay, false, true, false};
                emittedStart = true;
            }
            const double spanU = place.u1 - place.u0;
            const double spanT = place.t1 - place.t0;
            for (int i = 0; i < triggerCount && count < capacity; ++i) {
                if (!(spanU != 0.0) || !(spanT > 0.0))
                    continue;
                const double triggerU = triggers[i].u;
                const double fraction = (triggerU - place.u0) / spanU;
                if (!(fraction > 0.0 && fraction <= 1.0))
                    continue;
                const double when = place.t0 + fraction * spanT + delay;
                // Chunk edges are not exact in binary. A microsecond of slack keeps a
                // crossing on the shared boundary in the earlier chunk only.
                constexpr double edge = 1e-6;
                if (!(when > transport0 + edge && when <= transport1 + edge))
                    continue;
                out[count++] = Hit{i, place.dir, place.pass, triggerU, when, false, false, false};
            }
            if (segEnd >= place.t1 && count < capacity) {
                out[count++] = Hit{-1, place.dir, place.pass, place.u1, place.t1 + delay, true, false, false};
                if (once && place.pass == patternCount && count < capacity)
                    out[count++] = Hit{-1, 0, place.pass, place.u1, place.t1 + delay, false, false, true};
            }
            if (!(segEnd > t))
                break;
            t = segEnd;
        }
        return count;
    }

private:
    double cycleDuration(bool firstCycle) const noexcept {
        double total = 0;
        for (int i = 0; i < patternCount; ++i)
            total += stepDuration(pattern[i], firstCycle && i == 0);
        return total;
    }

};

}
