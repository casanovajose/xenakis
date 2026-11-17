#pragma once

#include "xen/Engine.h"

#include <functional>
#include <string>

namespace xen {

// Shared by the frame and xPlugin ui/ targets. No widgets live here.
struct EditorContext {
    Engine* engine = nullptr;
    std::string selection;
    int tool = 1;
    double originX = -1.0;
    double originY = -1.0;
    double pixelsPerUnit = 48.0;
    std::function<void()> changed;
    std::function<void()> save;
    std::function<double()> transportSeconds;
    std::function<double()> playheadUnits;
    std::function<std::string()> transportReadout;
    std::function<double()> bpm;
};

}
