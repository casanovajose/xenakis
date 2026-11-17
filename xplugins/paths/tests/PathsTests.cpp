#include "TransportXPlugin.h"
#include "rt/Geometry.h"
#include "rt/Timing.h"
#include "xen/Engine.h"
#include "xen/XPluginRegistration.h"
#include "xen/contracts/TempoMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

using namespace xen;
using Catch::Approx;

namespace {

std::unique_ptr<Engine> started() {
    auto engine = std::make_unique<Engine>();
    engine->setPlugins(createXPlugins());
    engine->startup();
    return engine;
}

struct Trig {
    double u = 0.5;
};

paths::rt::CursorMath gridMath(double length, double speed, const char* pattern) {
    int values[8]{};
    int count = 0;
    bool once = false;
    std::string text(pattern);
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ')
            ++i;
        if (i >= text.size())
            break;
        int value = 0;
        bool negative = false;
        if (text[i] == '-') {
            negative = true;
            ++i;
        }
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + (text[i] - '0');
            ++i;
        }
        if (negative)
            value = -value;
        if (value == 0) {
            once = true;
            break;
        }
        if (count < 8)
            values[count++] = value < 0 ? -1 : 1;
    }
    return paths::rt::CursorMath::make(length, 0.5, speed, 1.0, 0.0, 0.0, 0.0, 1.0, true, 0.0, values, count, once);
}

void addLine(Engine& engine) {
    engine.execute("AddGroup", Json{{"name", "Intro"}});
    engine.execute("AddPath", Json{{"name", "Line"}, {"points", Json::array({Json{{"x", 0.0}, {"y", 0.0}}, Json{{"x", 8.0}, {"y", 0.0}}})}});
    engine.execute("MoveToGroup", Json{{"entity", "P1"}, {"group", "G1"}});
    engine.execute("AddCursor", Json{{"path", "P1"}, {"name", "C1"}});
    engine.execute("AddTrigger", Json{{"path", "P1"}, {"u", 0.5}, {"name", "T1"}});
}

} // namespace

TEST_CASE("polyline and cubic arc length") {
    paths::rt::PathTable line;
    paths::rt::appendLine(line, {0, 0}, {8, 0});
    REQUIRE(line.length == Approx(8.0).margin(1e-9));
    const auto mid = line.pointAt(0.5);
    REQUIRE(mid.x == Approx(4.0).margin(1e-9));
    REQUIRE(mid.y == Approx(0.0).margin(1e-9));

    const paths::rt::Vec2 p0{0, 0};
    const paths::rt::Vec2 c1{2, 4};
    const paths::rt::Vec2 c2{6, -4};
    const paths::rt::Vec2 p1{8, 0};
    paths::rt::PathTable curve;
    paths::rt::appendCubic(curve, p0, c1, c2, p1, 64);
    const double fine = paths::rt::integrateCubic(p0, c1, c2, p1, 4096);
    REQUIRE(std::abs(curve.length - fine) / fine < 1e-3);
}

TEST_CASE("grid and absolute pass durations are exact") {
    const auto grid = gridMath(8.0, 1.0, "1 0");
    REQUIRE(grid.passDuration() == 4.0);
    int pattern[] = {1};
    const auto absolute = paths::rt::CursorMath::make(8.0, 0.5, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, false, 2.0, pattern, 1, true);
    REQUIRE(absolute.passDuration() == 2.0);
    const auto beats = paths::rt::CursorMath::make(8.0, 0.5, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, false, contracts::beatsToSeconds(2.0, 120.0),
                                                   pattern, 1, true);
    REQUIRE(beats.passDuration() == 1.0);
}

TEST_CASE("round trip pattern fires forward then back") {
    const auto math = gridMath(8.0, 1.0, "1 -1 0");
    Trig triggers[] = {{0.5}};
    paths::rt::Hit hits[32];
    const int count = math.crossings(0.0, 8.0, triggers, 1, hits, 32);
    std::vector<int> dirs;
    for (int i = 0; i < count; ++i) {
        if (!hits[i].passEnded && !hits[i].started && !hits[i].stopped && hits[i].triggerIndex >= 0)
            dirs.push_back(hits[i].dir);
    }
    REQUIRE(dirs == std::vector<int>{1, -1});
}

TEST_CASE("a cursor at 500 passes per second misses no crossings") {
    const auto math = gridMath(8.0, 2000.0, "1");
    REQUIRE(math.passDuration() == Approx(0.002).margin(1e-12));
    Trig triggers[] = {{0.5}};
    int fires = 0;
    for (int step = 0; step < 200; ++step) {
        const double t0 = step * 0.005;
        const double t1 = t0 + 0.005;
        paths::rt::Hit hits[32];
        const int count = math.crossings(t0, t1, triggers, 1, hits, 32);
        for (int i = 0; i < count; ++i) {
            if (hits[i].triggerIndex >= 0)
                ++fires;
        }
    }
    paths::rt::Hit whole[1200];
    const int wholeCount = math.crossings(0.0, 1.0, triggers, 1, whole, 1200);
    int wholeFires = 0;
    for (int i = 0; i < wholeCount; ++i) {
        if (whole[i].triggerIndex >= 0)
            ++wholeFires;
    }
    REQUIRE(wholeFires == 500);
    REQUIRE(fires == 500);
}

TEST_CASE("seeking recomputes the pose and skips crossings in the gap") {
    const auto math = gridMath(8.0, 1.0, "1 0");
    const auto before = math.placeAt(1.25);
    const auto again = math.placeAt(1.25);
    REQUIRE(before.u == again.u);
    REQUIRE(before.pass == again.pass);
    REQUIRE(before.dir == again.dir);
    REQUIRE(before.moving == again.moving);

    Trig triggers[] = {{0.5}};
    paths::rt::Hit hits[32];
    const int skipped = math.crossings(1.25, 1.25, triggers, 1, hits, 32);
    REQUIRE(skipped == 0);
    const int after = math.crossings(2.5, 4.0, triggers, 1, hits, 32);
    int fires = 0;
    for (int i = 0; i < after; ++i) {
        if (hits[i].triggerIndex >= 0)
            ++fires;
    }
    REQUIRE(fires == 0);
    const int beforeSeek = math.crossings(0.0, 2.5, triggers, 1, hits, 32);
    fires = 0;
    for (int i = 0; i < beforeSeek; ++i) {
        if (hits[i].triggerIndex >= 0)
            ++fires;
    }
    REQUIRE(fires == 1);
}

TEST_CASE("a live grid pass lasts four seconds") {
    auto engine = started();
    addLine(*engine);
    engine->setProperty("C1", "xen.paths.Cursor.pattern", "1 0");
    double ended = -1;
    engine->events().subscribe("cursor.passEnded", [&](const Event& event) {
        if (ended < 0.0)
            ended = event.payload.at("time").get<double>();
    });
    const auto& transport = *transport::TransportXPlugin::find(engine->plugins());
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    const auto wall0 = std::chrono::steady_clock::now();
    const double startSeconds = transport.reading().seconds;
    std::this_thread::sleep_for(std::chrono::milliseconds(4050));
    engine->pump();
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
    const double advanced = transport.reading().seconds - startSeconds;
    std::cout << "grid_pass_event=" << ended << " wall=" << wall << " advanced=" << advanced << '\n';
    engine->execute("transport.stop", Json::object());
    engine->stopClock();
    REQUIRE(ended == Approx(4.0).margin(0.001));
    REQUIRE(std::abs(advanced - wall) < 0.001);
}

TEST_CASE("a live absolute pass lasts two seconds") {
    auto engine = started();
    addLine(*engine);
    engine->setProperty("C1", "xen.paths.Cursor.timingMode", "absolute");
    engine->setProperty("C1", "xen.paths.Cursor.timingDuration", 2.0);
    engine->setProperty("C1", "xen.paths.Cursor.timingUnit", "s");
    engine->setProperty("C1", "xen.paths.Cursor.pattern", "1 0");
    double ended = -1;
    engine->events().subscribe("cursor.passEnded", [&](const Event& event) {
        if (ended < 0.0)
            ended = event.payload.at("time").get<double>();
    });
    const auto& transport = *transport::TransportXPlugin::find(engine->plugins());
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    const auto wall0 = std::chrono::steady_clock::now();
    const double startSeconds = transport.reading().seconds;
    std::this_thread::sleep_for(std::chrono::milliseconds(2050));
    engine->pump();
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
    const double advanced = transport.reading().seconds - startSeconds;
    std::cout << "absolute_pass_event=" << ended << " wall=" << wall << " advanced=" << advanced << '\n';
    engine->execute("transport.stop", Json::object());
    engine->stopClock();
    REQUIRE(ended == Approx(2.0).margin(0.001));
    REQUIRE(std::abs(advanced - wall) < 0.001);
}

TEST_CASE("save and reload keeps the same crossings") {
    auto engine = started();
    addLine(*engine);
    engine->setProperty("C1", "xen.paths.Cursor.pattern", "1 -1 0");
    const auto path = std::filesystem::temp_directory_path() / "xenakis-paths-reload.xenproj";
    engine->saveProject(path);
    auto reloaded = started();
    reloaded->loadProject(path);
    const Json* cursor = reloaded->document().findComponent("C1", "xen.paths", "Cursor");
    const Json* trigger = reloaded->document().findComponent("T1", "xen.paths", "Trigger");
    REQUIRE(cursor != nullptr);
    REQUIRE(trigger != nullptr);
    REQUIRE(cursor->at("pattern") == "1 -1 0");
    REQUIRE(trigger->at("u") == 0.5);
    const auto math = gridMath(8.0, 1.0, "1 -1 0");
    Trig triggers[] = {{0.5}};
    paths::rt::Hit hits[32];
    const int count = math.crossings(0.0, 8.0, triggers, 1, hits, 32);
    std::vector<int> dirs;
    for (int i = 0; i < count; ++i) {
        if (hits[i].triggerIndex >= 0)
            dirs.push_back(hits[i].dir);
    }
    REQUIRE(dirs == std::vector<int>{1, -1});
    std::filesystem::remove(path);
}

TEST_CASE("live round trip fires the trigger twice") {
    auto engine = started();
    addLine(*engine);
    engine->setProperty("C1", "xen.paths.Cursor.timingSpeed", 40.0);
    engine->setProperty("C1", "xen.paths.Cursor.pattern", "1 -1 0");
    std::vector<int> dirs;
    engine->events().subscribe("trigger.fired", [&](const Event& event) { dirs.push_back(event.payload.at("dir").get<int>()); });
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    engine->pump();
    engine->execute("transport.stop", Json::object());
    engine->stopClock();
    REQUIRE(dirs == std::vector<int>{1, -1});
}
