#include "PathsCanvas.h"

#include "rt/Geometry.h"
#include "rt/Timing.h"

#include <algorithm>
#include <cmath>

namespace xen::paths::ui {
namespace {
constexpr juce::uint32 kBack = 0xff141518;
constexpr juce::uint32 kGrid = 0xff2a2d33;
constexpr juce::uint32 kMajor = 0xff3a3e46;
constexpr juce::uint32 kPath = 0xffc8c8c8;
constexpr juce::uint32 kAccent = 0xff3d8bfd;
constexpr juce::uint32 kTrigger = 0xffffffff;
constexpr juce::uint32 kLabel = 0xff8a8f98;

rt::PathTable makeTable(const Json& points) {
    rt::PathTable table;
    rt::Vec2 previous{};
    bool has = false;
    if (!points.is_array())
        return table;
    for (const auto& point : points) {
        const rt::Vec2 next{point.value("x", 0.0), point.value("y", 0.0)};
        if (!has) {
            table.samples.push_back(rt::Sample{0.0, next.x, next.y});
            previous = next;
            has = true;
            continue;
        }
        if (point.contains("c1x")) {
            rt::appendCubic(table, previous, {point.value("c1x", previous.x), point.value("c1y", previous.y)},
                            {point.value("c2x", next.x), point.value("c2y", next.y)}, next, 24);
        } else {
            rt::appendLine(table, previous, next);
        }
        previous = next;
    }
    return table;
}

template <typename Screen>
juce::Path strokeOf(const Json& points, Screen toScreen) {
    juce::Path path;
    bool started = false;
    float px = 0;
    float py = 0;
    if (!points.is_array())
        return path;
    for (const auto& point : points) {
        const float x = point.value("x", 0.0f);
        const float y = point.value("y", 0.0f);
        const auto screen = toScreen(x, y);
        if (!started) {
            path.startNewSubPath(screen);
            started = true;
        } else if (point.contains("c1x")) {
            const auto c1 = toScreen(point.value("c1x", px), point.value("c1y", py));
            const auto c2 = toScreen(point.value("c2x", x), point.value("c2y", y));
            path.cubicTo(c1, c2, screen);
        } else {
            path.lineTo(screen);
        }
        px = x;
        py = y;
    }
    return path;
}
} // namespace

PathsCanvas::PathsCanvas(EditorContext& context) : context_(context) {
    setOpaque(true);
    if (context_.engine != nullptr) {
        triggerToken_ = context_.engine->events().subscribe("trigger.fired", [this](const Event& event) {
            flashId_ = event.payload.value("triggerId", std::string());
            flashed_ = std::chrono::steady_clock::now();
        });
    }
}

PathsCanvas::~PathsCanvas() {
    if (context_.engine != nullptr && triggerToken_ != 0)
        context_.engine->events().unsubscribe(triggerToken_);
}

juce::Point<float> PathsCanvas::toScreen(float x, float y) const {
    const float sx = static_cast<float>((x - context_.originX) * context_.pixelsPerUnit);
    const float sy = static_cast<float>(getHeight() - (y - context_.originY) * context_.pixelsPerUnit);
    return {sx, sy};
}

PathsCanvas::Point PathsCanvas::toGrid(juce::Point<float> pixel) const {
    Point point;
    point.x = static_cast<float>(pixel.x / context_.pixelsPerUnit + context_.originX);
    point.y = static_cast<float>((getHeight() - pixel.y) / context_.pixelsPerUnit + context_.originY);
    const float snap = 0.25f;
    point.x = std::round(point.x / snap) * snap;
    point.y = std::round(point.y / snap) * snap;
    return point;
}

std::string PathsCanvas::ensureGroup() {
    if (context_.engine == nullptr)
        return {};
    if (context_.selection.rfind("G", 0) == 0 && context_.engine->document().contains(context_.selection))
        return context_.selection;
    for (const auto& id : context_.engine->document().entities()) {
        if (context_.engine->document().findComponent(id, "xen.groups", "Group") != nullptr)
            return id;
    }
    context_.engine->execute("AddGroup", Json{{"name", "Group"}});
    for (const auto& id : context_.engine->document().entities()) {
        if (id.rfind("G", 0) == 0)
            return id;
    }
    return {};
}

bool PathsCanvas::nearestPath(Point grid, std::string& pathId, double& u) const {
    if (context_.engine == nullptr)
        return false;
    double best = 0.35;
    bool found = false;
    for (const auto& id : context_.engine->document().entities()) {
        const Json* path = context_.engine->document().findComponent(id, "xen.paths", "Path");
        if (path == nullptr || !path->contains("points"))
            continue;
        rt::PathTable table = makeTable(path->at("points"));
        if (!(table.length > 0.0))
            continue;
        for (int step = 0; step <= 40; ++step) {
            const double fraction = step / 40.0;
            const rt::Vec2 point = table.pointAt(fraction);
            const double dx = point.x - grid.x;
            const double dy = point.y - grid.y;
            const double distance = std::sqrt(dx * dx + dy * dy);
            if (distance < best) {
                best = distance;
                pathId = id;
                u = fraction;
                found = true;
            }
        }
    }
    return found;
}

void PathsCanvas::finishPath() {
    if (context_.engine == nullptr || draft_.size() < 2)
        return;
    const std::string group = ensureGroup();
    Json points = Json::array();
    for (const auto& point : draft_)
        points.push_back(Json{{"x", point.x}, {"y", point.y}});
    const auto before = context_.engine->document().entities();
    context_.engine->execute("AddPath", Json{{"name", "Path"}, {"points", points}});
    std::string created;
    for (const auto& id : context_.engine->document().entities()) {
        if (std::find(before.begin(), before.end(), id) == before.end() && id.rfind("P", 0) == 0)
            created = id;
    }
    if (!created.empty() && !group.empty())
        context_.engine->execute("MoveToGroup", Json{{"entity", created}, {"group", group}});
    context_.selection = created;
    draft_.clear();
    if (context_.changed)
        context_.changed();
}

void PathsCanvas::mouseDown(const juce::MouseEvent& event) {
    if (context_.engine == nullptr)
        return;
    const Point grid = toGrid(event.position);
    if (context_.tool == 1) {
        draft_.push_back(grid);
        if (event.mods.isRightButtonDown())
            finishPath();
        repaint();
        return;
    }
    std::string pathId;
    double u = 0;
    if (!nearestPath(grid, pathId, u))
        return;
    if (context_.tool == 2) {
        context_.engine->execute("AddCursor", Json{{"path", pathId}, {"name", "Cursor"}});
    } else if (context_.tool == 3) {
        context_.engine->execute("AddTrigger", Json{{"path", pathId}, {"name", "Trigger"}, {"u", u}});
    } else {
        context_.selection = pathId;
    }
    if (context_.changed)
        context_.changed();
}

void PathsCanvas::mouseDoubleClick(const juce::MouseEvent&) {
    if (context_.tool == 1)
        finishPath();
}

void PathsCanvas::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(kBack));
    const int width = getWidth();
    const int height = getHeight();
    graphics.setColour(juce::Colour(kGrid));
    for (int unit = -20; unit < 80; ++unit) {
        const auto major = toScreen(static_cast<float>(unit), 0);
        graphics.setColour(unit % 4 == 0 ? juce::Colour(kMajor) : juce::Colour(kGrid));
        graphics.drawVerticalLine(static_cast<int>(major.x), 0.0f, static_cast<float>(height));
        const auto horizontal = toScreen(0, static_cast<float>(unit));
        graphics.drawHorizontalLine(static_cast<int>(horizontal.y), 0.0f, static_cast<float>(width));
        if (unit % 4 == 0) {
            graphics.setColour(juce::Colour(kLabel));
            graphics.drawText(juce::String(unit), static_cast<int>(major.x) + 4, 4, 48, 16, juce::Justification::left);
        }
    }

    if (context_.engine == nullptr)
        return;
    for (const auto& id : context_.engine->document().entities()) {
        const Json* path = context_.engine->document().findComponent(id, "xen.paths", "Path");
        if (path == nullptr || !path->contains("points"))
            continue;
        auto stroke = strokeOf(path->at("points"), [this](float x, float y) { return toScreen(x, y); });
        graphics.setColour(id == context_.selection ? juce::Colour(kAccent) : juce::Colour(kPath));
        graphics.strokePath(stroke, juce::PathStrokeType(1.5f));
    }

    const auto now = std::chrono::steady_clock::now();
    const bool flashing = std::chrono::duration<double>(now - flashed_).count() < 0.15;
    for (const auto& id : context_.engine->document().entities()) {
        const Json* trigger = context_.engine->document().findComponent(id, "xen.paths", "Trigger");
        if (trigger == nullptr)
            continue;
        const auto pathId = trigger->value("path", std::string());
        const Json* path = context_.engine->document().findComponent(pathId, "xen.paths", "Path");
        if (path == nullptr || !path->contains("points"))
            continue;
        const rt::PathTable table = makeTable(path->at("points"));
        const rt::Vec2 point = table.pointAt(trigger->value("u", 0.5));
        const auto screen = toScreen(static_cast<float>(point.x), static_cast<float>(point.y));
        const bool hot = flashing && flashId_ == id;
        graphics.setColour(hot || id == context_.selection ? juce::Colour(kAccent) : juce::Colour(kTrigger));
        graphics.drawLine(screen.x, screen.y - 10.0f, screen.x, screen.y + 10.0f, 2.0f);
    }

    for (const auto& id : context_.engine->document().entities()) {
        const Json* cursor = context_.engine->document().findComponent(id, "xen.paths", "Cursor");
        if (cursor == nullptr)
            continue;
        const auto pathId = cursor->value("path", std::string());
        const Json* path = context_.engine->document().findComponent(pathId, "xen.paths", "Path");
        if (path == nullptr || !path->contains("points"))
            continue;
        const rt::PathTable table = makeTable(path->at("points"));
        const double seconds = context_.transportSeconds ? context_.transportSeconds() : 0.0;
        const double bpm = context_.bpm ? context_.bpm() : 120.0;
        const double grid = bpm > 0.0 ? 60.0 / bpm : 0.5;
        const double speed = cursor->value("timingSpeed", 1.0);
        const bool gridMode = cursor->value("timingMode", std::string("grid")) != "absolute";
        const double absolute = cursor->value("timingDuration", 2.0);
        int pattern[8] = {1};
        rt::CursorMath math = rt::CursorMath::make(table.length, grid, speed, cursor->value("speedFactor", 1.0),
                                                   cursor->value("delay", 0.0), cursor->value("start", 0.0),
                                                   cursor->value("loopIn", 0.0), cursor->value("loopOut", 1.0), gridMode,
                                                   absolute, pattern, 1, false);
        const auto place = math.placeAt(seconds - cursor->value("delay", 0.0));
        const rt::Vec2 point = table.pointAt(place.u);
        const auto screen = toScreen(static_cast<float>(point.x), static_cast<float>(point.y));
        graphics.setColour(juce::Colour(kAccent));
        graphics.fillEllipse(screen.x - 6.0f, screen.y - 6.0f, 12.0f, 12.0f);
    }

    if (context_.playheadUnits) {
        const auto head = toScreen(static_cast<float>(context_.playheadUnits()), 0);
        graphics.setColour(juce::Colour(kAccent).withAlpha(0.85f));
        graphics.drawVerticalLine(static_cast<int>(head.x), 0.0f, static_cast<float>(height));
    }

    if (!draft_.empty()) {
        juce::Path preview;
        preview.startNewSubPath(toScreen(draft_.front().x, draft_.front().y));
        for (std::size_t i = 1; i < draft_.size(); ++i)
            preview.lineTo(toScreen(draft_[i].x, draft_[i].y));
        graphics.setColour(juce::Colour(kAccent));
        graphics.strokePath(preview, juce::PathStrokeType(1.5f));
    }
}

} // namespace xen::paths::ui
