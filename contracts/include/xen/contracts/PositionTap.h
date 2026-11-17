#pragma once

namespace xen::rt {

struct CursorSample {
    char cursorId[16]{};
    char pathId[16]{};
    float u = 0;
    float x = 0;
    float y = 0;
    int pass = 0;
    double time = 0;
    bool moving = false;
    bool muted = false;
};

// Stable for the life of the paths xPlugin. The clock pushes at most one
// sample per cursor per tick. A sender thread pops. push failure drops.
class PositionTap {
public:
    virtual ~PositionTap() = default;
    virtual bool pop(CursorSample& sample) noexcept = 0;
};

}
