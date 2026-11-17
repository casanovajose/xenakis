#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace xen::paths::rt {

struct Vec2 {
    double x = 0;
    double y = 0;
};

struct Sample {
    double distance = 0;
    double x = 0;
    double y = 0;
};

struct PathTable {
    std::vector<Sample> samples;
    double length = 0;

    Vec2 pointAt(double u) const noexcept {
        if (samples.empty())
            return {};
        if (!(length > 0.0) || samples.size() == 1)
            return {samples.front().x, samples.front().y};
        double target = std::clamp(u, 0.0, 1.0) * length;
        std::size_t hi = 1;
        while (hi + 1 < samples.size() && samples[hi].distance < target)
            ++hi;
        const Sample& a = samples[hi - 1];
        const Sample& b = samples[hi];
        const double span = b.distance - a.distance;
        const double fraction = span > 0.0 ? (target - a.distance) / span : 0.0;
        return {a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction};
    }
};

inline Vec2 cubicPoint(Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p1, double t) noexcept {
    const double u = 1.0 - t;
    const double a = u * u * u;
    const double b = 3.0 * u * u * t;
    const double c = 3.0 * u * t * t;
    const double d = t * t * t;
    return {a * p0.x + b * c1.x + c * c2.x + d * p1.x, a * p0.y + b * c1.y + c * c2.y + d * p1.y};
}

inline void appendLine(PathTable& table, Vec2 from, Vec2 to) {
    if (table.samples.empty())
        table.samples.push_back(Sample{0.0, from.x, from.y});
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    table.length += std::sqrt(dx * dx + dy * dy);
    table.samples.push_back(Sample{table.length, to.x, to.y});
}

inline void appendCubic(PathTable& table, Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p1, int steps) {
    if (steps < 2)
        steps = 2;
    if (table.samples.empty())
        table.samples.push_back(Sample{0.0, p0.x, p0.y});
    Vec2 previous = p0;
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        const Vec2 point = cubicPoint(p0, c1, c2, p1, t);
        const double dx = point.x - previous.x;
        const double dy = point.y - previous.y;
        table.length += std::sqrt(dx * dx + dy * dy);
        table.samples.push_back(Sample{table.length, point.x, point.y});
        previous = point;
    }
}

inline double integrateCubic(Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p1, int steps) noexcept {
    if (steps < 2)
        steps = 2;
    double length = 0;
    Vec2 previous = p0;
    for (int i = 1; i <= steps; ++i) {
        const Vec2 point = cubicPoint(p0, c1, c2, p1, static_cast<double>(i) / static_cast<double>(steps));
        const double dx = point.x - previous.x;
        const double dy = point.y - previous.y;
        length += std::sqrt(dx * dx + dy * dy);
        previous = point;
    }
    return length;
}

}
