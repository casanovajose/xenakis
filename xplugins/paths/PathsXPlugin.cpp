#include "PathsXPlugin.h"

#include "xen/contracts/CompileHandoff.h"
#include "xen/contracts/TempoMath.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace xen::paths {
namespace {

constexpr const char* kOwner = "xen.paths";

double numberField(const Json& value, const char* key, double fallback) {
    if (!value.contains(key) || !value.at(key).is_number())
        return fallback;
    return value.at(key).get<double>();
}

Json defaultLook() {
    return Json{{"color", "#c8c8c8"}, {"width", 1.5}, {"dash", false}, {"visible", true}, {"label", true}};
}

Json defaultPath(const std::string& name, const Json& points) {
    return Json{{"name", name}, {"points", points}, {"closed", false}, {"look", defaultLook()}};
}

Json defaultCursor(const std::string& name, const std::string& path) {
    return Json{
        {"name", name},
        {"path", path},
        {"timingMode", "grid"},
        {"timingSpeed", 1.0},
        {"timingDuration", 2.0},
        {"timingUnit", "s"},
        {"pattern", "1"},
        {"delay", 0.0},
        {"start", 0.0},
        {"loopIn", 0.0},
        {"loopOut", 1.0},
        {"speedFactor", 1.0},
        {"armed", true},
        {"stream", false},
        {"look", Json{{"color", "#3d8bfd"}, {"size", 8.0}, {"shape", "circle"}, {"label", true}}},
    };
}

Json defaultTrigger(const std::string& name, const std::string& path, double u) {
    return Json{
        {"name", name},
        {"path", path},
        {"u", u},
        {"cursors", "all"},
        {"direction", "both"},
        {"enabled", true},
        {"oscAddress", ""},
        {"oscArgs", ""},
        {"look", Json{{"color", "#ffffff"}, {"size", 8.0}}},
    };
}

class BlobCommand : public Command {
public:
    void undo(CommandContext& context) override {
        if (!existed_)
            context.document.eraseOwner(entity_, kOwner);
        else
            context.document.setOwnerBlob(entity_, kOwner, before_);
    }

protected:
    void capture(const Document& document, const EntityId& entity) {
        if (captured_)
            return;
        entity_ = entity;
        before_ = document.ownerBlob(entity, kOwner);
        existed_ = !before_.is_null();
        captured_ = true;
    }

    EntityId entity_;
    Json before_;
    bool existed_ = false;
    bool captured_ = false;
};

class AddEntityCommand : public Command {
public:
    void undo(CommandContext& context) override { context.document.destroyEntity(id_); }

protected:
    EntityId create(CommandContext& context, const char* prefix) {
        if (!created_) {
            id_ = context.document.createEntity(prefix);
            created_ = true;
        } else if (!context.document.contains(id_)) {
            context.document.insertEntity(id_);
        }
        return id_;
    }

    EntityId id_;
    bool created_ = false;
};

class AddPathCommand final : public AddEntityCommand {
public:
    explicit AddPathCommand(Json args) {
        name_ = args.value("name", std::string("Path"));
        points_ = args.contains("points") && args["points"].is_array() ? args["points"] : Json::array();
        if (points_.empty())
            points_ = Json::array({Json{{"x", 0.0}, {"y", 0.0}}, Json{{"x", 1.0}, {"y", 0.0}}});
    }

    std::string_view name() const override { return "AddPath"; }

    void execute(CommandContext& context) override {
        const EntityId id = create(context, "P");
        context.document.setComponent(id, kOwner, "Path", defaultPath(name_, points_));
        context.events.publish(Event{"path.created", Json{{"id", id}}, false});
    }

private:
    std::string name_;
    Json points_;
};

class EditPathPointsCommand final : public BlobCommand {
public:
    explicit EditPathPointsCommand(Json args) {
        if (!args.contains("path") || !args.contains("points") || !args["points"].is_array())
            throw std::runtime_error("EditPathPoints needs path and points");
        path_ = args.at("path").get<std::string>();
        points_ = args.at("points");
        if (args.contains("closed"))
            closed_ = args.at("closed");
    }

    std::string_view name() const override { return "EditPathPoints"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(path_, kOwner, "Path") == nullptr)
            throw std::runtime_error("EditPathPoints target is not a path");
        capture(context.document, path_);
        Json updated = *context.document.findComponent(path_, kOwner, "Path");
        updated["points"] = points_;
        if (!closed_.is_null())
            updated["closed"] = closed_;
        context.document.setComponent(path_, kOwner, "Path", std::move(updated));
    }

private:
    EntityId path_;
    Json points_;
    Json closed_;
};

class AddCursorCommand final : public AddEntityCommand {
public:
    explicit AddCursorCommand(Json args) {
        if (!args.contains("path"))
            throw std::runtime_error("AddCursor needs a path");
        path_ = args.at("path").get<std::string>();
        name_ = args.value("name", std::string("Cursor"));
    }

    std::string_view name() const override { return "AddCursor"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(path_, kOwner, "Path") == nullptr)
            throw std::runtime_error("AddCursor path is missing");
        const EntityId id = create(context, "C");
        context.document.setComponent(id, kOwner, "Cursor", defaultCursor(name_, path_));
        context.events.publish(Event{"cursor.created", Json{{"id", id}, {"path", path_}}, false});
    }

private:
    EntityId path_;
    std::string name_;
};

class AddTriggerCommand final : public AddEntityCommand {
public:
    explicit AddTriggerCommand(Json args) {
        if (!args.contains("path"))
            throw std::runtime_error("AddTrigger needs a path");
        path_ = args.at("path").get<std::string>();
        name_ = args.value("name", std::string("Trigger"));
        u_ = args.value("u", 0.5);
    }

    std::string_view name() const override { return "AddTrigger"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(path_, kOwner, "Path") == nullptr)
            throw std::runtime_error("AddTrigger path is missing");
        const EntityId id = create(context, "T");
        context.document.setComponent(id, kOwner, "Trigger", defaultTrigger(name_, path_, u_));
        context.events.publish(Event{"trigger.created", Json{{"id", id}, {"path", path_}}, false});
    }

private:
    EntityId path_;
    std::string name_;
    double u_ = 0.5;
};

class MoveCursorCommand final : public BlobCommand {
public:
    explicit MoveCursorCommand(Json args) {
        if (!args.contains("cursor") || !args.contains("u"))
            throw std::runtime_error("MoveCursor needs cursor and u");
        cursor_ = args.at("cursor").get<std::string>();
        u_ = args.at("u").get<double>();
    }

    std::string_view name() const override { return "MoveCursor"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(cursor_, kOwner, "Cursor") == nullptr)
            throw std::runtime_error("MoveCursor target is not a cursor");
        capture(context.document, cursor_);
        Json cursor = *context.document.findComponent(cursor_, kOwner, "Cursor");
        cursor["start"] = u_;
        context.document.setComponent(cursor_, kOwner, "Cursor", std::move(cursor));
    }

private:
    EntityId cursor_;
    double u_ = 0;
};

class MoveTriggerCommand final : public BlobCommand {
public:
    explicit MoveTriggerCommand(Json args) {
        if (!args.contains("trigger") || !args.contains("u"))
            throw std::runtime_error("MoveTrigger needs trigger and u");
        trigger_ = args.at("trigger").get<std::string>();
        u_ = args.at("u").get<double>();
    }

    std::string_view name() const override { return "MoveTrigger"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(trigger_, kOwner, "Trigger") == nullptr)
            throw std::runtime_error("MoveTrigger target is not a trigger");
        capture(context.document, trigger_);
        Json trigger = *context.document.findComponent(trigger_, kOwner, "Trigger");
        trigger["u"] = u_;
        context.document.setComponent(trigger_, kOwner, "Trigger", std::move(trigger));
    }

private:
    EntityId trigger_;
    double u_ = 0;
};

class DeleteCommand final : public Command {
public:
    explicit DeleteCommand(Json args) {
        if (!args.contains("entity"))
            throw std::runtime_error("Delete needs an entity");
        entity_ = args.at("entity").get<std::string>();
    }

    std::string_view name() const override { return "Delete"; }

    void execute(CommandContext& context) override {
        if (!captured_) {
            before_ = context.document.ownerBlob(entity_, kOwner);
            captured_ = true;
        }
        context.document.eraseOwner(entity_, kOwner);
    }

    void undo(CommandContext& context) override {
        if (!before_.is_null())
            context.document.setOwnerBlob(entity_, kOwner, before_);
    }

private:
    EntityId entity_;
    Json before_;
    bool captured_ = false;
};

class ArmCommand final : public BlobCommand {
public:
    ArmCommand(Json args, bool armed) : armed_(armed) {
        if (!args.contains("cursor"))
            throw std::runtime_error("cursor.arm needs a cursor");
        cursor_ = args.at("cursor").get<std::string>();
    }

    std::string_view name() const override { return armed_ ? "cursor.arm" : "cursor.disarm"; }

    void execute(CommandContext& context) override {
        if (context.document.findComponent(cursor_, kOwner, "Cursor") == nullptr)
            throw std::runtime_error("cursor arm target is not a cursor");
        capture(context.document, cursor_);
        Json cursor = *context.document.findComponent(cursor_, kOwner, "Cursor");
        cursor["armed"] = armed_;
        context.document.setComponent(cursor_, kOwner, "Cursor", std::move(cursor));
    }

private:
    EntityId cursor_;
    bool armed_ = true;
};

PropertyDesc prop(const char* component, const char* field, ValueType type, Json fallback, std::optional<double> min = std::nullopt,
                  std::optional<double> max = std::nullopt) {
    return PropertyDesc{
        std::string(kOwner) + "." + component + "." + field, kOwner, component, field, type, std::move(fallback), min, max, field};
}

void parsePattern(const std::string& text, int* out, int& count, bool& once) {
    count = 0;
    once = false;
    std::stringstream stream(text);
    int value = 0;
    while (stream >> value) {
        if (value == 0) {
            once = true;
            break;
        }
        if (count < 8)
            out[count++] = value < 0 ? -1 : 1;
    }
}

int beatsPerBarOf(const Json* transport) {
    const auto meter = transport != nullptr ? transport->value("meter", std::string("4/4")) : std::string("4/4");
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

double absoluteSeconds(const Json& cursor, double bpm, int beatsPerBar) {
    const auto unit = cursor.value("timingUnit", std::string("s"));
    const double duration = numberField(cursor, "timingDuration", 2.0);
    if (unit == "ms")
        return duration / 1000.0;
    if (unit == "min")
        return duration * 60.0;
    if (unit == "beats")
        return contracts::beatsToSeconds(duration, bpm);
    if (unit == "bars")
        return contracts::beatsToSeconds(duration * static_cast<double>(beatsPerBar), bpm);
    return duration;
}

rt::PathTable buildTable(const Json& points) {
    rt::PathTable table;
    rt::Vec2 previous{};
    bool has = false;
    if (!points.is_array())
        return table;
    for (const auto& point : points) {
        if (!point.is_object())
            continue;
        const rt::Vec2 next{numberField(point, "x", 0.0), numberField(point, "y", 0.0)};
        if (!has) {
            table.samples.push_back(rt::Sample{0.0, next.x, next.y});
            previous = next;
            has = true;
            continue;
        }
        if (point.contains("c1x")) {
            rt::appendCubic(table, previous, {numberField(point, "c1x", previous.x), numberField(point, "c1y", previous.y)},
                            {numberField(point, "c2x", next.x), numberField(point, "c2y", next.y)}, next, 32);
        } else {
            rt::appendLine(table, previous, next);
        }
        previous = next;
    }
    return table;
}

bool chainMuted(const Document& document, std::string id) {
    for (int guard = 0; guard < 32; ++guard) {
        const Json* group = document.findComponent(id, "xen.groups", "Group");
        if (group != nullptr && group->value("mute", false))
            return true;
        const Json* member = document.findComponent(id, "xen.groups", "GroupMember");
        if (member == nullptr || !member->contains("parent") || !member->at("parent").is_string())
            return false;
        id = member->at("parent").get<std::string>();
    }
    return false;
}

std::string parentGroup(const Document& document, const std::string& id) {
    const Json* member = document.findComponent(id, "xen.groups", "GroupMember");
    if (member != nullptr && member->contains("parent") && member->at("parent").is_string())
        return member->at("parent").get<std::string>();
    return {};
}

const Json* transportJson(const Document& document) {
    for (const auto& id : document.entities()) {
        const Json* transport = document.findComponent(id, "xen.transport", "Transport");
        if (transport != nullptr)
            return transport;
    }
    return nullptr;
}

} // namespace

XPluginInfo PathsXPlugin::info() const {
    return XPluginInfo{"xen.paths", "Paths", 1, {Dependency{"xen.transport"}}, {Dependency{"xen.groups"}, Dependency{"xen.osc"}}};
}

void PathsXPlugin::contribute(Registry& registry) {
    registry.components().add(kOwner, "Path");
    registry.components().add(kOwner, "Cursor");
    registry.components().add(kOwner, "Trigger");
    registry.properties().add(prop("Path", "name", ValueType::String, ""));
    registry.properties().add(prop("Path", "closed", ValueType::Bool, false));
    registry.properties().add(prop("Cursor", "name", ValueType::String, ""));
    registry.properties().add(prop("Cursor", "timingMode", ValueType::String, "grid"));
    registry.properties().add(prop("Cursor", "timingSpeed", ValueType::Float, 1.0, 0.0, 1000000.0));
    registry.properties().add(prop("Cursor", "timingDuration", ValueType::Float, 2.0, 0.0, 1000000.0));
    registry.properties().add(prop("Cursor", "timingUnit", ValueType::String, "s"));
    registry.properties().add(prop("Cursor", "pattern", ValueType::String, "1"));
    registry.properties().add(prop("Cursor", "delay", ValueType::Float, 0.0));
    registry.properties().add(prop("Cursor", "start", ValueType::Float, 0.0, 0.0, 1.0));
    registry.properties().add(prop("Cursor", "loopIn", ValueType::Float, 0.0, 0.0, 1.0));
    registry.properties().add(prop("Cursor", "loopOut", ValueType::Float, 1.0, 0.0, 1.0));
    registry.properties().add(prop("Cursor", "speedFactor", ValueType::Float, 1.0, 0.0, 1000000.0));
    registry.properties().add(prop("Cursor", "armed", ValueType::Bool, true));
    registry.properties().add(prop("Cursor", "stream", ValueType::Bool, false));
    registry.properties().add(prop("Trigger", "name", ValueType::String, ""));
    registry.properties().add(prop("Trigger", "u", ValueType::Float, 0.5, 0.0, 1.0));
    registry.properties().add(prop("Trigger", "direction", ValueType::String, "both"));
    registry.properties().add(prop("Trigger", "enabled", ValueType::Bool, true));
    registry.properties().add(prop("Trigger", "oscAddress", ValueType::String, ""));
    registry.properties().add(prop("Trigger", "oscArgs", ValueType::String, ""));
    registry.commands().add("AddPath", kOwner, [](const Json& args) { return std::make_unique<AddPathCommand>(args); });
    registry.commands().add("EditPathPoints", kOwner, [](const Json& args) { return std::make_unique<EditPathPointsCommand>(args); });
    registry.commands().add("AddCursor", kOwner, [](const Json& args) { return std::make_unique<AddCursorCommand>(args); });
    registry.commands().add("MoveCursor", kOwner, [](const Json& args) { return std::make_unique<MoveCursorCommand>(args); });
    registry.commands().add("AddTrigger", kOwner, [](const Json& args) { return std::make_unique<AddTriggerCommand>(args); });
    registry.commands().add("MoveTrigger", kOwner, [](const Json& args) { return std::make_unique<MoveTriggerCommand>(args); });
    registry.commands().add("Delete", kOwner, [](const Json& args) { return std::make_unique<DeleteCommand>(args); });
    registry.commands().add("DeletePath", kOwner, [](const Json& args) {
        Json forwarded = args;
        if (!forwarded.contains("entity") && forwarded.contains("path"))
            forwarded["entity"] = forwarded["path"];
        return std::make_unique<DeleteCommand>(std::move(forwarded));
    });
    registry.commands().add("cursor.arm", kOwner, [](const Json& args) { return std::make_unique<ArmCommand>(args, true); });
    registry.commands().add("cursor.disarm", kOwner, [](const Json& args) { return std::make_unique<ArmCommand>(args, false); });
    registry.addAction(kOwner, "cursor.arm");
    registry.addAction(kOwner, "cursor.disarm");
    registry.addAction(kOwner, "cursor.solo");
    for (const char* event : {"path.created", "cursor.created", "trigger.created", "trigger.fired", "cursor.started",
                              "cursor.passEnded", "cursor.stopped", "cursor.position"})
        registry.addEvent(kOwner, event);
}

void PathsXPlugin::activate(Document&) {
    contracts::publishPositionTap(this);
}

void PathsXPlugin::deactivate(Document&) {
    if (contracts::positionTap() == this)
        contracts::publishPositionTap(nullptr);
}

void PathsXPlugin::compile(const Document& document, RtBuilder& builder) {
    const Json* transport = transportJson(document);
    const double bpm = transport != nullptr ? numberField(*transport, "bpm", 120.0) : 120.0;
    const bool bpmMode = transport == nullptr || transport->value("timeMode", std::string("bpm")) != "absolute";
    const int perBar = beatsPerBarOf(transport);
    const double grid = contracts::gridUnitToSeconds(
        bpmMode, transport != nullptr ? numberField(*transport, "gridUnit", 1.0) : 1.0, bpm,
        contracts::absoluteUnitSeconds(transport != nullptr ? transport->value("absoluteUnit", std::string("s")).c_str() : "s"));

    std::vector<std::string> pathIds;
    std::vector<rt::PathTable> tables;
    for (const auto& id : document.entities()) {
        const Json* path = document.findComponent(id, kOwner, "Path");
        if (path == nullptr)
            continue;
        pathIds.push_back(id);
        tables.push_back(buildTable(path->contains("points") ? path->at("points") : Json::array()));
    }

    std::vector<rt::CursorConfig> cursors;
    for (const auto& id : document.entities()) {
        const Json* cursor = document.findComponent(id, kOwner, "Cursor");
        if (cursor == nullptr)
            continue;
        const auto pathId = cursor->value("path", std::string());
        int pathIndex = -1;
        for (int i = 0; i < static_cast<int>(pathIds.size()); ++i) {
            if (pathIds[static_cast<std::size_t>(i)] == pathId)
                pathIndex = i;
        }
        if (pathIndex < 0)
            continue;
        int pattern[8]{};
        int count = 0;
        bool once = false;
        parsePattern(cursor->value("pattern", std::string("1")), pattern, count, once);
        const bool gridMode = cursor->value("timingMode", std::string("grid")) != "absolute";
        rt::CursorConfig config;
        rt::copyId(config.id, id.c_str());
        rt::copyId(config.pathId, pathId.c_str());
        rt::copyId(config.groupId, parentGroup(document, pathId).c_str());
        config.pathIndex = pathIndex;
        config.math = rt::CursorMath::make(
            tables[static_cast<std::size_t>(pathIndex)].length, grid, numberField(*cursor, "timingSpeed", 1.0),
            numberField(*cursor, "speedFactor", 1.0), numberField(*cursor, "delay", 0.0), numberField(*cursor, "start", 0.0),
            numberField(*cursor, "loopIn", 0.0), numberField(*cursor, "loopOut", 1.0), gridMode,
            absoluteSeconds(*cursor, bpm, perBar), pattern, count, once);
        config.armed = cursor->value("armed", true);
        config.stream = cursor->value("stream", false);
        config.muted = chainMuted(document, pathId) || chainMuted(document, id);
        cursors.push_back(config);
    }

    std::vector<rt::TriggerPoint> triggers;
    for (const auto& id : document.entities()) {
        const Json* trigger = document.findComponent(id, kOwner, "Trigger");
        if (trigger == nullptr)
            continue;
        const auto pathId = trigger->value("path", std::string());
        int pathIndex = -1;
        for (int i = 0; i < static_cast<int>(pathIds.size()); ++i) {
            if (pathIds[static_cast<std::size_t>(i)] == pathId)
                pathIndex = i;
        }
        rt::TriggerPoint point;
        rt::copyId(point.id, id.c_str());
        point.u = numberField(*trigger, "u", 0.5);
        const auto direction = trigger->value("direction", std::string("both"));
        point.direction = direction == "fwd" ? 1 : direction == "back" ? -1 : 0;
        point.enabled = trigger->value("enabled", true);
        point.pathIndex = pathIndex;
        point.muted = chainMuted(document, pathId);
        point.allCursors = true;
        if (trigger->contains("cursors") && trigger->at("cursors").is_array()) {
            point.allCursors = false;
            for (const auto& cursorId : trigger->at("cursors")) {
                if (cursorId.is_string() && point.cursorCount < 8)
                    rt::copyId(point.cursors[point.cursorCount++], cursorId.get<std::string>().c_str());
            }
        }
        triggers.push_back(point);
    }

    builder.add(std::make_unique<rt::PathsNode>(std::move(tables), std::move(cursors), std::move(triggers),
                                                contracts::tempoSource(), slots_, 128, &notices_, &positions_));
}

void PathsXPlugin::pump(EventBus& events) {
    rt::Notice notice;
    while (notices_.pop(notice)) {
        const bool muted = notice.muted;
        if (notice.kind == rt::NoticeKind::Trigger) {
            events.publish(Event{
                "trigger.fired",
                Json{{"triggerId", notice.triggerId},
                     {"cursorId", notice.cursorId},
                     {"pathId", notice.pathId},
                     {"groupId", notice.groupId},
                     {"dir", notice.dir},
                     {"pass", notice.pass},
                     {"u", notice.u},
                     {"x", notice.x},
                     {"y", notice.y},
                     {"time", notice.time}},
                muted,
            });
        } else if (notice.kind == rt::NoticeKind::Started) {
            events.publish(Event{"cursor.started", Json{{"cursorId", notice.cursorId}, {"time", notice.time}}, muted});
        } else if (notice.kind == rt::NoticeKind::Pass) {
            events.publish(Event{
                "cursor.passEnded",
                Json{{"cursorId", notice.cursorId}, {"pass", notice.pass}, {"time", notice.time}},
                muted,
            });
        } else if (notice.kind == rt::NoticeKind::Stopped) {
            events.publish(Event{"cursor.stopped", Json{{"cursorId", notice.cursorId}, {"time", notice.time}}, muted});
        }
    }
}

void PathsXPlugin::migrate(Json& data, int fromVersion) {
    if (fromVersion >= 1 || !data.is_object())
        return;
    if (data.contains("Path") && data["Path"].is_object()) {
        Json& path = data["Path"];
        if (!path.contains("points"))
            path["points"] = Json::array();
        if (!path.contains("closed"))
            path["closed"] = false;
        if (!path.contains("name"))
            path["name"] = "";
    }
}

bool PathsXPlugin::pop(xen::rt::CursorSample& sample) noexcept {
    return positions_.pop(sample);
}

} // namespace xen::paths
