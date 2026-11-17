#pragma once

#include "xen/EditorContext.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <chrono>
#include <string>
#include <vector>

namespace xen::paths::ui {

class PathsCanvas final : public juce::Component {
public:
    explicit PathsCanvas(EditorContext& context);
    ~PathsCanvas() override;
    void paint(juce::Graphics& graphics) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    void finishPath();

private:
    struct Point {
        float x = 0;
        float y = 0;
    };

    juce::Point<float> toScreen(float x, float y) const;
    Point toGrid(juce::Point<float> pixel) const;
    std::string ensureGroup();
    bool nearestPath(Point grid, std::string& pathId, double& u) const;

    EditorContext& context_;
    std::vector<Point> draft_;
    std::string flashId_;
    std::chrono::steady_clock::time_point flashed_{};
    int triggerToken_ = 0;
};

}
