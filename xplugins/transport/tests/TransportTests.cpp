#include "TransportXPlugin.h"
#include "xen/Engine.h"
#include "xen/XPluginRegistration.h"
#include "xen/contracts/TempoMath.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace xen;

namespace {

std::unique_ptr<Engine> started() {
    auto engine = std::make_unique<Engine>();
    engine->setPlugins(createXPlugins());
    engine->startup();
    return engine;
}

const transport::TransportXPlugin& transportOf(const Engine& engine) {
    const auto* plugin = transport::TransportXPlugin::find(engine.plugins());
    REQUIRE(plugin != nullptr);
    return *plugin;
}

const Json& transportJson(const Document& document) {
    const Json* transport = document.findComponent("transport", "xen.transport", "Transport");
    REQUIRE(transport != nullptr);
    return *transport;
}

} // namespace

TEST_CASE("seconds and beats convert exactly at a constant tempo") {
    REQUIRE(contracts::beatsToSeconds(2.0, 120.0) == 1.0);
    REQUIRE(contracts::secondsToBeats(1.0, 120.0) == 2.0);
    REQUIRE(contracts::beatsToSeconds(contracts::secondsToBeats(4.0, 120.0), 120.0) == 4.0);
    REQUIRE(contracts::beatsToSeconds(1.0, 60.0) == 1.0);
    REQUIRE(contracts::gridUnitToSeconds(true, 1.0, 120.0, 1.0) == 0.5);
    REQUIRE(contracts::gridUnitToSeconds(false, 1.0, 120.0, 1.0) == 1.0);

    auto engine = started();
    const auto& transport = transportOf(*engine);
    engine->execute("transport.seek", Json{{"time", 1.0}});
    const auto atOne = transport.reading();
    REQUIRE(atOne.seconds == 1.0);
    REQUIRE(atOne.beats == 2.0);
    REQUIRE(atOne.gridUnitSeconds == 0.5);
    REQUIRE(transport.beatsToSeconds(8.0) == 4.0);

    engine->setProperty("transport", "xen.transport.bpm", 60.0);
    engine->execute("transport.seek", Json{{"time", 2.0}});
    const auto atTwo = transport.reading();
    REQUIRE(atTwo.bpm == 60.0);
    REQUIRE(atTwo.seconds == 2.0);
    REQUIRE(atTwo.beats == 2.0);
    REQUIRE(atTwo.gridUnitSeconds == 1.0);
}

TEST_CASE("play pause stop and seek") {
    auto engine = started();
    const auto& transport = transportOf(*engine);
    int started = 0;
    int paused = 0;
    int stopped = 0;
    int seeked = 0;
    engine->events().subscribe("transport.started", [&](const Event&) { ++started; });
    engine->events().subscribe("transport.paused", [&](const Event&) { ++paused; });
    engine->events().subscribe("transport.stopped", [&](const Event&) { ++stopped; });
    engine->events().subscribe("transport.seeked", [&](const Event&) { ++seeked; });

    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    REQUIRE(std::string(transport.reading().state) == "playing");
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    engine->execute("transport.pause", Json::object());
    const double frozen = transport.reading().seconds;
    REQUIRE(std::string(transport.reading().state) == "paused");
    REQUIRE(frozen > 0.04);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    REQUIRE(transport.reading().seconds == frozen);

    engine->execute("transport.seek", Json{{"time", 1.5}});
    REQUIRE(transport.reading().seconds == 1.5);
    REQUIRE(transport.reading().beats == 3.0);
    REQUIRE(std::string(transport.reading().state) == "paused");

    engine->execute("transport.play", Json::object());
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    engine->execute("transport.pause", Json::object());
    REQUIRE(transport.reading().seconds > 1.55);

    engine->execute("transport.stop", Json::object());
    REQUIRE(std::string(transport.reading().state) == "stopped");
    REQUIRE(transport.reading().seconds == 0.0);
    REQUIRE(started == 2);
    REQUIRE(paused == 2);
    REQUIRE(stopped == 1);
    REQUIRE(seeked == 1);

    REQUIRE(engine->undo());
    REQUIRE(std::string(transport.reading().state) == "paused");
    engine->stopClock();
}

TEST_CASE("loop range wraps back to the in point") {
    auto engine = started();
    const auto& transport = transportOf(*engine);
    engine->setProperty("transport", "xen.transport.loopIn", 0.0);
    engine->setProperty("transport", "xen.transport.loopOut", 0.2);
    engine->execute("transport.toggleLoop", Json::object());
    REQUIRE(transportJson(engine->document()).at("loop") == true);

    int wraps = 0;
    engine->events().subscribe("transport.looped", [&](const Event& event) {
        wraps += event.payload.value("wraps", 1);
    });

    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    engine->pump();
    const auto reading = transport.reading();
    engine->execute("transport.pause", Json::object());
    REQUIRE(reading.seconds >= 0.0);
    REQUIRE(reading.seconds < 0.2);
    REQUIRE(wraps >= 2);

    engine->execute("transport.stop", Json::object());
    REQUIRE(transport.reading().seconds == 0.0);
    engine->setProperty("transport", "xen.transport.loopIn", 0.4);
    engine->execute("transport.stop", Json::object());
    REQUIRE(transport.reading().seconds == 0.4);
    engine->stopClock();
}

TEST_CASE("transport position tracks the wall clock") {
    const char* overrideSeconds = std::getenv("XEN_DRIFT_SECONDS");
    const double window = overrideSeconds != nullptr ? std::atof(overrideSeconds) : 2.0;
    REQUIRE(window >= 0.5);

    auto engine = started();
    const auto& transport = transportOf(*engine);
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    const auto start = std::chrono::steady_clock::now();
    const double startSeconds = transport.reading().seconds;
    std::this_thread::sleep_for(std::chrono::duration<double>(window));
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double drifted = std::abs((transport.reading().seconds - startSeconds) - elapsed);
    std::cout << "drift_seconds=" << drifted << " elapsed=" << elapsed << '\n';
    engine->execute("transport.stop", Json::object());
    engine->stopClock();
    REQUIRE(drifted < 0.001);
}

TEST_CASE("tempo change is notified and the position readout follows the meter") {
    auto engine = started();
    const auto& transport = transportOf(*engine);
    int changes = 0;
    engine->events().subscribe("transport.tempoChanged", [&](const Event& event) {
        REQUIRE(event.payload.at("bpm") == 90.0);
        ++changes;
    });
    engine->setProperty("transport", "xen.transport.bpm", 90.0);
    REQUIRE(changes == 1);
    engine->execute("transport.seek", Json{{"time", 2.0}});
    REQUIRE(transport.formatPosition() == "0001:4:000");
}
