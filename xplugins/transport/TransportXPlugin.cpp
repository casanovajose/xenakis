#include "TransportXPlugin.h"

#include "xen/contracts/CompileHandoff.h"
#include "xen/contracts/TempoMath.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace xen::transport {
namespace {

constexpr const char* kOwner = "xen.transport";
constexpr const char* kEntity = "transport";

Json defaultTransport() {
    return Json{
        {"timeMode", "bpm"},
        {"bpm", 120.0},
        {"meter", "4/4"},
        {"absoluteUnit", "s"},
        {"gridUnit", 1.0},
        {"loop", false},
        {"loopIn", 0.0},
        {"loopOut", 4.0},
        {"state", "stopped"},
        {"position", 0.0},
        {"epoch", 0},
        {"jumped", false},
    };
}

int beatsPerBar(const Json& transport) {
    const auto meter = transport.value("meter", std::string("4/4"));
    const auto slash = meter.find('/');
    if (slash == std::string::npos || slash == 0)
        return 4;
    try {
        const int beats = std::stoi(meter.substr(0, slash));
        return beats > 0 ? beats : 4;
    } catch (const std::exception&) {
        return 4;
    }
}

double numberField(const Json& transport, const char* key, double fallback) {
    if (!transport.contains(key) || !transport.at(key).is_number())
        return fallback;
    return transport.at(key).get<double>();
}

rt::Settings settingsFrom(const Json& transport) {
    rt::Settings settings;
    settings.bpm = numberField(transport, "bpm", 120.0);
    settings.gridUnit = numberField(transport, "gridUnit", 1.0);
    settings.loopIn = numberField(transport, "loopIn", 0.0);
    settings.loopOut = numberField(transport, "loopOut", 4.0);
    settings.position = numberField(transport, "position", 0.0);
    settings.loop = transport.value("loop", false);
    settings.bpmMode = transport.value("timeMode", std::string("bpm")) != "absolute";
    settings.absoluteUnitSeconds = contracts::absoluteUnitSeconds(transport.value("absoluteUnit", std::string("s")).c_str());
    settings.jumped = transport.value("jumped", false);
    settings.epoch = transport.value("epoch", 0);
    settings.beatsPerBar = beatsPerBar(transport);
    const auto state = transport.value("state", std::string("stopped"));
    if (state == "playing")
        settings.state = static_cast<int>(rt::RunState::Playing);
    else if (state == "paused")
        settings.state = static_cast<int>(rt::RunState::Paused);
    else
        settings.state = static_cast<int>(rt::RunState::Stopped);
    return settings;
}

Json transportJson(const Document& document) {
    const Json* transport = document.findComponent(kEntity, kOwner, "Transport");
    if (transport == nullptr || !transport->is_object())
        throw std::runtime_error("transport is missing");
    return *transport;
}

void writeTransport(Document& document, Json transport) {
    document.setComponent(kEntity, kOwner, "Transport", std::move(transport));
}

class BlobCommand : public Command {
public:
    void undo(CommandContext& context) override {
        context.document.setOwnerBlob(kEntity, kOwner, before_);
    }

protected:
    void capture(const Document& document) {
        if (captured_)
            return;
        before_ = document.ownerBlob(kEntity, kOwner);
        captured_ = true;
    }

    Json before_;
    bool captured_ = false;
};

class PlayCommand final : public BlobCommand {
public:
    explicit PlayCommand(rt::TransportClock& clock) : clock_(clock) {}
    std::string_view name() const override { return "transport.play"; }

    void execute(CommandContext& context) override {
        capture(context.document);
        Json transport = transportJson(context.document);
        transport["position"] = clock_.read().seconds;
        transport["state"] = "playing";
        transport["epoch"] = transport.value("epoch", 0) + 1;
        transport["jumped"] = false;
        writeTransport(context.document, transport);
        context.events.publish(Event{"transport.started", Json{{"seconds", transport["position"]}, {"beats", clock_.read().beats}}, false});
    }

private:
    rt::TransportClock& clock_;
};

class PauseCommand final : public BlobCommand {
public:
    explicit PauseCommand(rt::TransportClock& clock) : clock_(clock) {}
    std::string_view name() const override { return "transport.pause"; }

    void execute(CommandContext& context) override {
        capture(context.document);
        const double seconds = clock_.read().seconds;
        Json transport = transportJson(context.document);
        transport["position"] = seconds;
        transport["state"] = "paused";
        transport["epoch"] = transport.value("epoch", 0) + 1;
        transport["jumped"] = false;
        writeTransport(context.document, transport);
        context.events.publish(Event{
            "transport.paused",
            Json{{"seconds", seconds}, {"beats", contracts::secondsToBeats(seconds, numberField(transport, "bpm", 120.0))}},
            false});
    }

private:
    rt::TransportClock& clock_;
};

class StopCommand final : public BlobCommand {
public:
    explicit StopCommand(rt::TransportClock& clock) : clock_(clock) {}
    std::string_view name() const override { return "transport.stop"; }

    void execute(CommandContext& context) override {
        capture(context.document);
        Json transport = transportJson(context.document);
        const double position = transport.value("loop", false) ? numberField(transport, "loopIn", 0.0) : 0.0;
        transport["position"] = position;
        transport["state"] = "stopped";
        transport["epoch"] = transport.value("epoch", 0) + 1;
        transport["jumped"] = false;
        writeTransport(context.document, transport);
        context.events.publish(Event{
            "transport.stopped",
            Json{{"seconds", position}, {"beats", contracts::secondsToBeats(position, numberField(transport, "bpm", 120.0))}},
            false});
        (void)clock_;
    }

private:
    rt::TransportClock& clock_;
};

class SeekCommand final : public BlobCommand {
public:
    SeekCommand(rt::TransportClock& clock, Json args) : clock_(clock) {
        if (!args.contains("time") || !args.at("time").is_number())
            throw std::runtime_error("transport.seek needs a numeric time");
        time_ = args.at("time").get<double>();
        if (time_ < 0.0)
            throw std::runtime_error("transport.seek time is negative");
    }

    std::string_view name() const override { return "transport.seek"; }

    void execute(CommandContext& context) override {
        capture(context.document);
        Json transport = transportJson(context.document);
        transport["position"] = time_;
        transport["epoch"] = transport.value("epoch", 0) + 1;
        transport["jumped"] = true;
        writeTransport(context.document, transport);
        context.events.publish(Event{
            "transport.seeked",
            Json{{"seconds", time_}, {"beats", contracts::secondsToBeats(time_, numberField(transport, "bpm", 120.0))}},
            false});
        (void)clock_;
    }

private:
    rt::TransportClock& clock_;
    double time_ = 0;
};

class ToggleLoopCommand final : public BlobCommand {
public:
    std::string_view name() const override { return "transport.toggleLoop"; }

    void execute(CommandContext& context) override {
        capture(context.document);
        Json transport = transportJson(context.document);
        transport["loop"] = !transport.value("loop", false);
        writeTransport(context.document, transport);
    }
};

PropertyDesc property(std::string field, ValueType type, Json fallback, std::optional<double> min = std::nullopt,
                      std::optional<double> max = std::nullopt) {
    return PropertyDesc{
        std::string(kOwner) + "." + field, kOwner, "Transport", field, type, std::move(fallback), min, max, field};
}

} // namespace

XPluginInfo TransportXPlugin::info() const {
    return XPluginInfo{"xen.transport", "Transport", 1, {}, {Dependency{"xen.osc"}}};
}

void TransportXPlugin::contribute(Registry& registry) {
    registry.components().add(kOwner, "Transport");
    registry.properties().add(property("timeMode", ValueType::String, "bpm"));
    registry.properties().add(property("bpm", ValueType::Float, 120.0, 20.0, 999.0));
    registry.properties().add(property("meter", ValueType::String, "4/4"));
    registry.properties().add(property("absoluteUnit", ValueType::String, "s"));
    registry.properties().add(property("gridUnit", ValueType::Float, 1.0, 0.000001, 1000000.0));
    registry.properties().add(property("loop", ValueType::Bool, false));
    registry.properties().add(property("loopIn", ValueType::Float, 0.0));
    registry.properties().add(property("loopOut", ValueType::Float, 4.0));
    registry.commands().add("transport.play", kOwner, [this](const Json&) {
        return std::make_unique<PlayCommand>(clock_);
    });
    registry.commands().add("transport.pause", kOwner, [this](const Json&) {
        return std::make_unique<PauseCommand>(clock_);
    });
    registry.commands().add("transport.stop", kOwner, [this](const Json&) {
        return std::make_unique<StopCommand>(clock_);
    });
    registry.commands().add("transport.seek", kOwner, [this](const Json& args) {
        return std::make_unique<SeekCommand>(clock_, args);
    });
    registry.commands().add("transport.toggleLoop", kOwner, [](const Json&) {
        return std::make_unique<ToggleLoopCommand>();
    });
    for (const char* action : {"transport.play", "transport.pause", "transport.stop", "transport.seek", "transport.toggleLoop"})
        registry.addAction(kOwner, action);
    for (const char* event : {"transport.started", "transport.paused", "transport.stopped", "transport.seeked",
                              "transport.looped", "transport.tempoChanged", "transport.beat"})
        registry.addEvent(kOwner, event);
}

void TransportXPlugin::ensure(Document& document) const {
    if (!document.contains(kEntity))
        document.insertEntity(kEntity);
    if (document.findComponent(kEntity, kOwner, "Transport") == nullptr)
        document.setComponent(kEntity, kOwner, "Transport", defaultTransport());
}

void TransportXPlugin::activate(Document& document) {
    ensure(document);
}

void TransportXPlugin::afterLoad(Document& document) {
    ensure(document);
    Json transport = transportJson(document);
    if (transport.value("state", std::string("stopped")) != "stopped") {
        transport["state"] = "stopped";
        transport["jumped"] = false;
        transport["epoch"] = transport.value("epoch", 0) + 1;
        writeTransport(document, std::move(transport));
    }
    clock_.invalidate();
}

void TransportXPlugin::compile(const Document& document, RtBuilder& builder) {
    const Json* transport = document.findComponent(kEntity, kOwner, "Transport");
    const rt::Settings settings = transport != nullptr ? settingsFrom(*transport) : rt::Settings{};
    if (publishedBpm_ >= 0.0 && (settings.bpm != publishedBpm_ || settings.bpmMode != publishedBpmMode_)) {
        tempoDirty_ = true;
        tempoBpm_ = settings.bpm;
        tempoBpmMode_ = settings.bpmMode;
    }
    publishedBpm_ = settings.bpm;
    publishedBpmMode_ = settings.bpmMode;
    clock_.apply(settings);
    auto node = std::make_unique<rt::TransportNode>(&clock_);
    contracts::publishTempoSource(node.get());
    builder.add(std::move(node));
}

void TransportXPlugin::onCompiled(EventBus& events) {
    if (!tempoDirty_)
        return;
    tempoDirty_ = false;
    events.publish(Event{
        "transport.tempoChanged",
        Json{{"bpm", tempoBpm_}, {"timeMode", tempoBpmMode_ ? "bpm" : "absolute"}},
        false,
    });
}

void TransportXPlugin::pump(EventBus& events) {
    rt::Notice notice;
    while (clock_.pop(notice)) {
        if (notice.kind == rt::NoticeKind::Looped) {
            events.publish(Event{
                "transport.looped",
                Json{{"seconds", notice.seconds}, {"beats", notice.beats}, {"wraps", notice.wraps}},
                false,
            });
        } else if (notice.kind == rt::NoticeKind::Beat) {
            events.publish(Event{
                "transport.beat",
                Json{{"bar", notice.bar}, {"beat", notice.beat}, {"seconds", notice.seconds}, {"beats", notice.beats}},
                false,
            });
        }
    }
}

void TransportXPlugin::migrate(Json& data, int fromVersion) {
    if (fromVersion >= 1 || !data.is_object())
        return;
    if (!data.contains("Transport") || !data["Transport"].is_object())
        data["Transport"] = defaultTransport();
    Json& transport = data["Transport"];
    const Json defaults = defaultTransport();
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        if (!transport.contains(it.key()))
            transport[it.key()] = it.value();
    }
}

TransportReading TransportXPlugin::reading() const {
    const rt::Readout readout = clock_.read();
    TransportReading reading;
    reading.seconds = readout.seconds;
    reading.beats = readout.beats;
    reading.bpm = readout.bpm;
    reading.gridUnitSeconds = readout.gridUnitSeconds;
    reading.beatsPerBar = readout.beatsPerBar;
    reading.loop = readout.loop;
    reading.bpmMode = readout.bpmMode;
    if (readout.state == static_cast<int>(rt::RunState::Playing))
        reading.state = "playing";
    else if (readout.state == static_cast<int>(rt::RunState::Paused))
        reading.state = "paused";
    else
        reading.state = "stopped";
    return reading;
}

std::string TransportXPlugin::formatPosition() const {
    const TransportReading now = reading();
    char text[32];
    if (!now.bpmMode) {
        double seconds = now.seconds;
        if (seconds < 0.0)
            seconds = 0.0;
        const auto totalMs = static_cast<int>(std::llround(seconds * 1000.0));
        const int minutes = totalMs / 60000;
        const int wholeSeconds = (totalMs / 1000) % 60;
        const int millis = totalMs % 1000;
        std::snprintf(text, sizeof(text), "%02d:%02d.%03d", minutes, wholeSeconds, millis);
        return text;
    }
    double beats = now.beats;
    if (beats < 0.0)
        beats = 0.0;
    const int perBar = now.beatsPerBar > 0 ? now.beatsPerBar : 4;
    const int whole = static_cast<int>(std::floor(beats));
    const int bar = whole / perBar + 1;
    const int beat = whole % perBar + 1;
    const int ticks = static_cast<int>(std::floor((beats - static_cast<double>(whole)) * 1000.0));
    std::snprintf(text, sizeof(text), "%04d:%d:%03d", bar, beat, ticks);
    return text;
}

double TransportXPlugin::beatsToSeconds(double beats) const {
    return contracts::beatsToSeconds(beats, reading().bpm);
}

double TransportXPlugin::secondsToBeats(double seconds) const {
    return contracts::secondsToBeats(seconds, reading().bpm);
}

const TransportXPlugin* TransportXPlugin::find(const XPluginRegistry& plugins) {
    return dynamic_cast<const TransportXPlugin*>(plugins.find("xen.transport"));
}

} // namespace xen::transport
