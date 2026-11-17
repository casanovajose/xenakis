#include "OscXPlugin.h"

#include "xen/Version.h"
#include "xen/contracts/CompileHandoff.h"

#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace xen::osc {
namespace {

constexpr const char* kOwner = "xen.osc";
constexpr const char* kEntity = "osc";

void copyText(char* dest, std::size_t size, const std::string& text) {
    if (size == 0)
        return;
    const std::size_t count = text.size() < size - 1 ? text.size() : size - 1;
    std::memcpy(dest, text.data(), count);
    dest[count] = '\0';
}

Json defaultSettings() {
    return Json{
        {"destinations", Json::array({Json{{"id", "main"}, {"host", "127.0.0.1"}, {"port", 9001}, {"enabled", true}}})},
        {"prefix", "/xen"},
        {"streamRateHz", 30},
        {"useBundles", true},
        {"latencyMs", 10},
    };
}

void setString(Arg& arg, const std::string& text) {
    arg = Arg{};
    arg.type = 's';
    copyText(arg.text, sizeof(arg.text), text);
}

void addArg(Outgoing& message, Arg arg) {
    if (message.argc < 8)
        message.args[message.argc++] = arg;
}

double numberOr(const Json& payload, const char* key, double fallback) {
    if (!payload.contains(key) || !payload.at(key).is_number())
        return fallback;
    return payload.at(key).get<double>();
}

std::string payloadString(const Json& payload, const char* key) {
    if (!payload.contains(key) || !payload.at(key).is_string())
        return {};
    return payload.at(key).get<std::string>();
}

class TestCommand final : public Command {
public:
    explicit TestCommand(OscXPlugin& plugin) : plugin_(plugin) {}
    std::string_view name() const override { return "osc.test"; }
    void execute(CommandContext&) override { plugin_.sendTest(); }
    void undo(CommandContext&) override {}

private:
    OscXPlugin& plugin_;
};

class PanicCommand final : public Command {
public:
    explicit PanicCommand(OscXPlugin& plugin) : plugin_(plugin) {}
    std::string_view name() const override { return "osc.panic"; }
    void execute(CommandContext&) override { plugin_.panic(); }
    void undo(CommandContext&) override {}

private:
    OscXPlugin& plugin_;
};

class SetDestinationCommand final : public Command {
public:
    explicit SetDestinationCommand(Json args) {
        host_ = args.value("host", std::string("127.0.0.1"));
        port_ = args.value("port", 9001);
        enabled_ = args.value("enabled", true);
    }

    std::string_view name() const override { return "osc.setDestination"; }

    void execute(CommandContext& context) override {
        const Json* settings = context.document.findComponent(kEntity, kOwner, "OscSettings");
        if (settings == nullptr)
            throw std::runtime_error("OSC settings are missing");
        if (!captured_) {
            before_ = *settings;
            captured_ = true;
        }
        Json updated = *settings;
        updated["destinations"] = Json::array({Json{{"id", "main"}, {"host", host_}, {"port", port_}, {"enabled", enabled_}}});
        context.document.setComponent(kEntity, kOwner, "OscSettings", std::move(updated));
    }

    void undo(CommandContext& context) override {
        context.document.setComponent(kEntity, kOwner, "OscSettings", before_);
    }

private:
    std::string host_;
    int port_ = 9001;
    bool enabled_ = true;
    Json before_;
    bool captured_ = false;
};

PropertyDesc prop(const char* field, ValueType type, Json fallback, std::optional<double> min = std::nullopt,
                  std::optional<double> max = std::nullopt) {
    return PropertyDesc{std::string(kOwner) + "." + field, kOwner, "OscSettings", field, type, std::move(fallback), min, max, field};
}

} // namespace

OscXPlugin::OscXPlugin() = default;

OscXPlugin::~OscXPlugin() {
    running_.store(false);
    if (thread_.joinable())
        thread_.join();
}

XPluginInfo OscXPlugin::info() const {
    return XPluginInfo{"xen.osc", "OSC", 1, {}, {}};
}

void OscXPlugin::contribute(Registry& registry) {
    registry.components().add(kOwner, "OscSettings");
    registry.properties().add(prop("prefix", ValueType::String, "/xen"));
    registry.properties().add(prop("streamRateHz", ValueType::Int, 30, 0.0, 1000.0));
    registry.properties().add(prop("useBundles", ValueType::Bool, true));
    registry.properties().add(prop("latencyMs", ValueType::Float, 10.0, 0.0, 1000.0));
    registry.commands().add("osc.test", kOwner, [this](const Json&) { return std::make_unique<TestCommand>(*this); });
    registry.commands().add("osc.panic", kOwner, [this](const Json&) { return std::make_unique<PanicCommand>(*this); });
    registry.commands().add("osc.setDestination", kOwner, [](const Json& args) { return std::make_unique<SetDestinationCommand>(args); });
    registry.addAction(kOwner, "osc.test");
    registry.addAction(kOwner, "osc.panic");
    registry.addEvent(kOwner, "osc.sent");
    registry.addEvent(kOwner, "osc.error");
}

void OscXPlugin::ensure(Document& document) {
    if (!document.contains(kEntity))
        document.insertEntity(kEntity);
    if (document.findComponent(kEntity, kOwner, "OscSettings") == nullptr)
        document.setComponent(kEntity, kOwner, "OscSettings", defaultSettings());
}

void OscXPlugin::activate(Document& document) {
    document_ = &document;
    ensure(document);
    refresh(document);
    running_.store(true);
    thread_ = std::thread([this] { threadMain(); });
}

void OscXPlugin::deactivate(Document&) {
    if (events_ != nullptr) {
        for (int i = 0; i < tokenCount_; ++i)
            events_->unsubscribe(tokens_[i]);
    }
    tokenCount_ = 0;
    subscribed_ = false;
    events_ = nullptr;
    running_.store(false);
    if (thread_.joinable())
        thread_.join();
    document_ = nullptr;
}

void OscXPlugin::afterLoad(Document& document) {
    document_ = &document;
    ensure(document);
    refresh(document);
}

void OscXPlugin::compile(const Document& document, RtBuilder&) {
    refresh(document);
}

void OscXPlugin::onCompiled(EventBus& events) {
    subscribe(events);
}

void OscXPlugin::refresh(const Document& document) {
    const Json* settings = document.findComponent(kEntity, kOwner, "OscSettings");
    std::lock_guard<std::mutex> lock(settingsLock_);
    copyText(prefix_, sizeof(prefix_), settings != nullptr ? settings->value("prefix", std::string("/xen")) : std::string("/xen"));
    latency_ = settings != nullptr ? numberOr(*settings, "latencyMs", 10.0) / 1000.0 : 0.01;
    bundles_ = settings == nullptr || settings->value("useBundles", true);
    streamHz_ = settings != nullptr && settings->contains("streamRateHz") ? settings->value("streamRateHz", 30) : 30;
    destinationCount_ = 0;
    if (settings != nullptr && settings->contains("destinations") && settings->at("destinations").is_array()) {
        for (const auto& destination : settings->at("destinations")) {
            if (!destination.is_object() || destinationCount_ >= 4)
                continue;
            copyText(hosts_[destinationCount_], sizeof(hosts_[destinationCount_]), destination.value("host", std::string("127.0.0.1")));
            ports_[destinationCount_] = destination.value("port", 9001);
            enabled_[destinationCount_] = destination.value("enabled", true);
            ++destinationCount_;
        }
    }
}

void OscXPlugin::subscribe(EventBus& events) {
    if (subscribed_)
        return;
    events_ = &events;
    const auto add = [this](const char* name, EventBus::Handler handler) {
        if (tokenCount_ < 8)
            tokens_[tokenCount_++] = events_->subscribe(name, std::move(handler));
    };
    add("trigger.fired", [this](const Event& event) { onTrigger(event); });
    add("cursor.started", [this](const Event& event) { onCursor("start", event, false); });
    add("cursor.passEnded", [this](const Event& event) { onCursor("pass", event, true); });
    add("cursor.stopped", [this](const Event& event) { onCursor("stop", event, false); });
    add("transport.started", [this](const Event& event) { onTransport("play", event); });
    add("transport.paused", [this](const Event& event) { onTransport("pause", event); });
    add("transport.stopped", [this](const Event& event) { onTransport("stop", event); });
    add("transport.beat", [this](const Event& event) {
        if (event.muted)
            return;
        Outgoing message;
        {
            std::lock_guard<std::mutex> lock(settingsLock_);
            copyText(message.address, sizeof(message.address), std::string(prefix_) + "/transport/beat");
            message.bundle = bundles_;
        }
        Arg bar{};
        bar.type = 'i';
        bar.integer = event.payload.value("bar", 1);
        Arg beat{};
        beat.type = 'i';
        beat.integer = event.payload.value("beat", 1);
        addArg(message, bar);
        addArg(message, beat);
        const double time = numberOr(event.payload, "seconds", 0.0);
        if (const auto* tempo = contracts::tempoSource())
            message.unixSeconds = tempo->wallUnixAt(time) + latency_;
        enqueue(message);
    });
    subscribed_ = true;
}

void OscXPlugin::enqueue(Outgoing message) {
    if (!outgoing_.push(message))
        dropped_.fetch_add(1);
}

void OscXPlugin::onTrigger(const Event& event) {
    if (event.muted || document_ == nullptr)
        return;
    const auto triggerId = payloadString(event.payload, "triggerId");
    const auto cursorId = payloadString(event.payload, "cursorId");
    const auto pathId = payloadString(event.payload, "pathId");
    const double time = numberOr(event.payload, "time", 0.0);
    std::string address;
    std::string args;
    if (const Json* trigger = document_->findComponent(triggerId, "xen.paths", "Trigger")) {
        address = trigger->value("oscAddress", std::string());
        args = trigger->value("oscArgs", std::string());
    }
    Outgoing message;
    {
        std::lock_guard<std::mutex> lock(settingsLock_);
        if (address.empty())
            address = std::string(prefix_) + "/trigger/" + triggerId;
        message.bundle = bundles_;
        message.unixSeconds = latency_;
    }
    if (const auto* tempo = contracts::tempoSource())
        message.unixSeconds += tempo->wallUnixAt(time);
    copyText(message.address, sizeof(message.address), address);
    if (args.empty()) {
        Arg cursor{};
        setString(cursor, cursorId);
        Arg path{};
        setString(path, pathId);
        Arg dir{};
        dir.type = 'i';
        dir.integer = event.payload.value("dir", 1);
        Arg pass{};
        pass.type = 'i';
        pass.integer = event.payload.value("pass", 1);
        Arg u{};
        u.type = 'f';
        u.number = static_cast<float>(numberOr(event.payload, "u", 0.0));
        Arg x{};
        x.type = 'f';
        x.number = static_cast<float>(numberOr(event.payload, "x", 0.0));
        Arg y{};
        y.type = 'f';
        y.number = static_cast<float>(numberOr(event.payload, "y", 0.0));
        Arg stamp{};
        stamp.type = 'd';
        stamp.time = time;
        addArg(message, cursor);
        addArg(message, path);
        addArg(message, dir);
        addArg(message, pass);
        addArg(message, u);
        addArg(message, x);
        addArg(message, y);
        addArg(message, stamp);
    } else {
        std::stringstream stream(args);
        std::string token;
        while (stream >> token) {
            Arg arg{};
            if (token == "$cursor")
                setString(arg, cursorId);
            else if (token == "$path")
                setString(arg, pathId);
            else if (token == "$dir") {
                arg.type = 'i';
                arg.integer = event.payload.value("dir", 1);
            } else if (token == "$pass") {
                arg.type = 'i';
                arg.integer = event.payload.value("pass", 1);
            } else if (token == "$u") {
                arg.type = 'f';
                arg.number = static_cast<float>(numberOr(event.payload, "u", 0.0));
            } else if (token == "$x") {
                arg.type = 'f';
                arg.number = static_cast<float>(numberOr(event.payload, "x", 0.0));
            } else if (token == "$y") {
                arg.type = 'f';
                arg.number = static_cast<float>(numberOr(event.payload, "y", 0.0));
            } else if (token == "$t") {
                arg.type = 'd';
                arg.time = time;
            } else if (token.find('.') != std::string::npos) {
                arg.type = 'f';
                arg.number = std::stof(token);
            } else if (!token.empty() && (token[0] == '-' || (token[0] >= '0' && token[0] <= '9'))) {
                arg.type = 'i';
                arg.integer = std::stoi(token);
            } else {
                setString(arg, token);
            }
            addArg(message, arg);
        }
    }
    enqueue(message);
}

void OscXPlugin::onCursor(const char* suffix, const Event& event, bool withPass) {
    if (event.muted)
        return;
    const auto cursorId = payloadString(event.payload, "cursorId");
    const double time = numberOr(event.payload, "time", 0.0);
    Outgoing message;
    {
        std::lock_guard<std::mutex> lock(settingsLock_);
        copyText(message.address, sizeof(message.address), std::string(prefix_) + "/cursor/" + cursorId + "/" + suffix);
        message.bundle = bundles_;
        message.unixSeconds = latency_;
    }
    if (const auto* tempo = contracts::tempoSource())
        message.unixSeconds += tempo->wallUnixAt(time);
    if (withPass) {
        Arg pass{};
        pass.type = 'i';
        pass.integer = event.payload.value("pass", 1);
        addArg(message, pass);
    }
    Arg stamp{};
    stamp.type = 'd';
    stamp.time = time;
    addArg(message, stamp);
    enqueue(message);
}

void OscXPlugin::onTransport(const char* state, const Event& event) {
    Outgoing message;
    const double seconds = numberOr(event.payload, "seconds", 0.0);
    const double beats = numberOr(event.payload, "beats", 0.0);
    double bpm = 120;
    if (document_ != nullptr) {
        for (const auto& id : document_->entities()) {
            if (const Json* transport = document_->findComponent(id, "xen.transport", "Transport")) {
                bpm = numberOr(*transport, "bpm", 120);
                break;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(settingsLock_);
        copyText(message.address, sizeof(message.address), std::string(prefix_) + "/transport");
        message.bundle = bundles_;
        message.unixSeconds = latency_;
    }
    if (const auto* tempo = contracts::tempoSource())
        message.unixSeconds += tempo->wallUnixAt(seconds);
    Arg stateArg{};
    setString(stateArg, state);
    Arg secondsArg{};
    secondsArg.type = 'd';
    secondsArg.time = seconds;
    Arg beatsArg{};
    beatsArg.type = 'f';
    beatsArg.number = static_cast<float>(beats);
    Arg bpmArg{};
    bpmArg.type = 'f';
    bpmArg.number = static_cast<float>(bpm);
    addArg(message, stateArg);
    addArg(message, secondsArg);
    addArg(message, beatsArg);
    addArg(message, bpmArg);
    enqueue(message);
}

void OscXPlugin::sendTest() {
    streamHalted_ = false;
    Outgoing message;
    {
        std::lock_guard<std::mutex> lock(settingsLock_);
        copyText(message.address, sizeof(message.address), std::string(prefix_) + "/ping");
        message.bundle = bundles_;
        message.unixSeconds = latency_;
    }
    const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    message.unixSeconds += now;
    Arg name{};
    setString(name, "xenakis");
    Arg version{};
    setString(version, xen::kVersion);
    addArg(message, name);
    addArg(message, version);
    enqueue(message);
}

void OscXPlugin::panic() {
    streamHalted_ = true;
}

void OscXPlugin::pump(EventBus& events) {
    subscribe(events);
    SentNote note;
    while (sent_.pop(note)) {
        events.publish(Event{"osc.sent", Json{{"text", note.text}}, false});
    }
}

void OscXPlugin::migrate(Json& data, int fromVersion) {
    if (fromVersion >= 1 || !data.is_object())
        return;
    if (!data.contains("OscSettings"))
        data["OscSettings"] = defaultSettings();
}

void OscXPlugin::threadMain() {
    UdpSender sender;
    sender.open();
    auto lastStream = std::chrono::steady_clock::now();
    while (running_.load()) {
        Outgoing message;
        if (outgoing_.pop(message)) {
            const auto packet = encodePacket(message);
            char hosts[4][48]{};
            int ports[4]{};
            bool enabled[4]{};
            int count = 0;
            {
                std::lock_guard<std::mutex> lock(settingsLock_);
                count = destinationCount_;
                for (int i = 0; i < count; ++i) {
                    std::memcpy(hosts[i], hosts_[i], sizeof(hosts[i]));
                    ports[i] = ports_[i];
                    enabled[i] = enabled_[i];
                }
            }
            bool sent = false;
            for (int i = 0; i < count; ++i) {
                if (!enabled[i])
                    continue;
                if (sender.send(hosts[i], ports[i], packet.data(), static_cast<int>(packet.size())))
                    sent = true;
            }
            SentNote note{};
            copyText(note.text, sizeof(note.text), message.address);
            if (!sent)
                copyText(note.text, sizeof(note.text), std::string(message.address) + " error");
            sent_.push(note);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (streamHalted_)
            continue;
        int hz = 30;
        {
            std::lock_guard<std::mutex> lock(settingsLock_);
            hz = streamHz_;
        }
        if (hz <= 0)
            continue;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastStream < std::chrono::duration<double>(1.0 / static_cast<double>(hz)))
            continue;
        auto* tap = contracts::positionTap();
        if (tap == nullptr)
            continue;
        xen::rt::CursorSample sample{};
        xen::rt::CursorSample latest{};
        bool have = false;
        while (tap->pop(sample)) {
            latest = sample;
            have = true;
        }
        if (!have || latest.muted || !latest.moving)
            continue;
        lastStream = now;
        Outgoing stream;
        {
            std::lock_guard<std::mutex> lock(settingsLock_);
            copyText(stream.address, sizeof(stream.address), std::string(prefix_) + "/cursor/" + latest.cursorId);
            stream.bundle = bundles_;
            stream.unixSeconds = latency_;
        }
        const double nowUnix = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        stream.unixSeconds += nowUnix;
        Arg path{};
        setString(path, latest.pathId);
        Arg u{};
        u.type = 'f';
        u.number = latest.u;
        Arg x{};
        x.type = 'f';
        x.number = latest.x;
        Arg y{};
        y.type = 'f';
        y.number = latest.y;
        Arg pass{};
        pass.type = 'i';
        pass.integer = latest.pass;
        Arg stamp{};
        stamp.type = 'd';
        stamp.time = latest.time;
        addArg(stream, path);
        addArg(stream, u);
        addArg(stream, x);
        addArg(stream, y);
        addArg(stream, pass);
        addArg(stream, stamp);
        enqueue(stream);
    }
    sender.close();
}

} // namespace xen::osc
