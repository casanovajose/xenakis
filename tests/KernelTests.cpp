#include "xen/ControlClock.h"
#include "xen/Engine.h"
#include "xen/RtChannel.h"
#include "xen/Version.h"
#include "xen/XPluginRegistration.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

using namespace xen;

namespace {

std::unique_ptr<Engine> startedWithGroups() {
    auto engine = std::make_unique<Engine>();
    engine->setPlugins(createXPlugins());
    engine->startup();
    return engine;
}

const Json& groupComponent(const Document& document, const EntityId& id) {
    const Json* group = document.findComponent(id, "xen.groups", "Group");
    REQUIRE(group != nullptr);
    return *group;
}

class Marker final : public RtObject {
public:
    explicit Marker(int value) : value(value) {}
    int value = 0;
};

class MarkerPlugin final : public XPlugin {
public:
    XPluginInfo info() const override { return {"lab.marker", "Marker", 1, {}, {}}; }
    void contribute(Registry&) override {}
    void compile(const Document&, RtBuilder& builder) override {
        builder.add(std::make_unique<Marker>(++compiles));
    }
    int compiles = 0;
};

class NamedPlugin final : public XPlugin {
public:
    explicit NamedPlugin(XPluginInfo info) : info_(std::move(info)) {}
    XPluginInfo info() const override { return info_; }
    void contribute(Registry& registry) override {
        contributed = true;
        registry.addEvent(info_.id, "lab.ping");
    }
    void activate(Document&) override { activated = true; }
    bool contributed = false;
    bool activated = false;

private:
    XPluginInfo info_;
};

} // namespace

TEST_CASE("document ids, links, and json round trip") {
    Document document;
    const auto first = document.createEntity("E");
    const auto second = document.createEntity("E");
    REQUIRE(first == "E1");
    REQUIRE(second == "E2");
    document.setComponent(first, "lab.owner", "Note", Json{{"text", "a"}});
    document.addLink(Link{"lab/example", first, {second}});

    Document loaded;
    loaded.replaceFromJson(document.toJson({{"lab.owner", 1}}));
    REQUIRE(loaded.projectId() == document.projectId());
    REQUIRE(loaded.entities() == document.entities());
    REQUIRE(loaded.findComponent(first, "lab.owner", "Note")->at("text") == "a");
    REQUIRE(loaded.links().size() == 1);
    REQUIRE(loaded.links().at(0).to.at(0) == second);
}

TEST_CASE("property undo redo save and load") {
    auto engine = startedWithGroups();
    engine->execute("AddGroup", Json{{"name", "Intro"}});
    REQUIRE(groupComponent(engine->document(), "G1").at("visible") == true);

    int changes = 0;
    engine->events().subscribe("property.changed", [&](const Event& event) {
        if (event.payload.at("property") == "xen.groups.visible")
            ++changes;
    });

    engine->setProperty("G1", "xen.groups.visible", false);
    REQUIRE(groupComponent(engine->document(), "G1").at("visible") == false);
    REQUIRE(engine->undo());
    REQUIRE(groupComponent(engine->document(), "G1").at("visible") == true);
    REQUIRE(engine->redo());
    REQUIRE(groupComponent(engine->document(), "G1").at("visible") == false);
    REQUIRE(changes == 3);

    const auto path = std::filesystem::temp_directory_path() / "xenakis-property-roundtrip.xenproj";
    engine->saveProject(path);

    auto loaded = startedWithGroups();
    loaded->loadProject(path);
    REQUIRE(groupComponent(loaded->document(), "G1").at("visible") == false);
    REQUIRE(loaded->document().projectId() == engine->document().projectId());
    std::filesystem::remove(path);
}

TEST_CASE("unknown property does not enter the undo stack") {
    auto engine = startedWithGroups();
    engine->execute("AddGroup", Json{{"name", "Intro"}});
    REQUIRE_THROWS(engine->setProperty("G1", "xen.groups.nope", true));
    REQUIRE(engine->undo());
    REQUIRE(engine->document().entities().empty());
}

TEST_CASE("property range is enforced") {
    auto engine = startedWithGroups();
    const auto id = engine->document().createEntity("E");
    engine->document().setComponent(id, "lab.probe", "Thing", Json{{"gain", 1}});
    engine->properties().add(PropertyDesc{
        "lab.probe.gain", "lab.probe", "Thing", "gain", ValueType::Int, 1, 0.0, 10.0, "gain"});
    engine->setProperty(id, "lab.probe.gain", 4);
    REQUIRE(engine->document().findComponent(id, "lab.probe", "Thing")->at("gain") == 4);
    REQUIRE_THROWS(engine->setProperty(id, "lab.probe.gain", 11));
    REQUIRE(engine->document().findComponent(id, "lab.probe", "Thing")->at("gain") == 4);
}

TEST_CASE("opaque xplugin data and migration survive save and load") {
    auto engine = startedWithGroups();
    engine->execute("AddGroup", Json{{"name", "Intro"}});
    const auto path = std::filesystem::temp_directory_path() / "xenakis-opaque.xenproj";
    engine->saveProject(path);

    std::ifstream input(path);
    Json project;
    input >> project;
    input.close();
    project["xplugins"]["xen.groups"] = 0;
    project["entities"][0]["components"]["xen.groups"]["Group"].erase("visible");
    project["entities"][0]["components"]["xen.future"] = Json{{"keep", true}, {"n", 7}};
    std::ofstream output(path);
    output << project.dump(2);
    output.close();

    auto loaded = startedWithGroups();
    loaded->loadProject(path);
    REQUIRE(groupComponent(loaded->document(), "G1").at("visible") == true);
    REQUIRE(loaded->document().ownerBlob("G1", "xen.future").at("keep") == true);

    const auto saved = std::filesystem::temp_directory_path() / "xenakis-opaque-out.xenproj";
    loaded->saveProject(saved);
    Json again;
    {
        std::ifstream check(saved);
        check >> again;
    }
    REQUIRE(again["entities"][0]["components"]["xen.future"]["n"] == 7);
    REQUIRE(again["xplugins"]["xen.groups"] == 1);
    std::filesystem::remove(path);
    std::filesystem::remove(saved);
}

TEST_CASE("dependency resolution activates required plugins first") {
    PropertyRegistry properties;
    CommandRegistry commands;
    ComponentRegistry components;
    Registry contribute(properties, commands, components);
    Document document;

    auto base = std::make_unique<NamedPlugin>(XPluginInfo{"lab.base", "Base", 1, {}, {}});
    auto extra = std::make_unique<NamedPlugin>(
        XPluginInfo{"lab.extra", "Extra", 2, {Dependency{"lab.base"}}, {}});
    auto* basePtr = base.get();
    auto* extraPtr = extra.get();

    std::vector<std::unique_ptr<XPlugin>> plugins;
    plugins.push_back(std::move(extra));
    plugins.push_back(std::move(base));

    XPluginRegistry registry;
    registry.setPlugins(std::move(plugins));
    registry.startup(contribute, document);
    REQUIRE(registry.order() == std::vector<std::string>{"lab.base", "lab.extra"});
    REQUIRE(basePtr->activated);
    REQUIRE(extraPtr->activated);
    REQUIRE(contribute.events().size() == 2);

    PropertyRegistry missingProperties;
    CommandRegistry missingCommands;
    ComponentRegistry missingComponents;
    Registry missingRegistry(missingProperties, missingCommands, missingComponents);
    XPluginRegistry missing;
    std::vector<std::unique_ptr<XPlugin>> broken;
    broken.push_back(std::make_unique<NamedPlugin>(
        XPluginInfo{"lab.lonely", "Lonely", 1, {Dependency{"lab.missing"}}, {}}));
    missing.setPlugins(std::move(broken));
    REQUIRE_THROWS(missing.startup(missingRegistry, document));

    PropertyRegistry cycleProperties;
    CommandRegistry cycleCommands;
    ComponentRegistry cycleComponents;
    Registry cycleRegistry(cycleProperties, cycleCommands, cycleComponents);
    XPluginRegistry cycle;
    std::vector<std::unique_ptr<XPlugin>> loop;
    loop.push_back(std::make_unique<NamedPlugin>(
        XPluginInfo{"lab.a", "A", 1, {Dependency{"lab.b"}}, {}}));
    loop.push_back(std::make_unique<NamedPlugin>(
        XPluginInfo{"lab.b", "B", 1, {Dependency{"lab.a"}}, {}}));
    cycle.setPlugins(std::move(loop));
    REQUIRE_THROWS(cycle.startup(cycleRegistry, document));
}

TEST_CASE("compile swaps snapshots") {
    auto plugin = std::make_unique<MarkerPlugin>();
    auto* pluginPtr = plugin.get();
    Engine engine;
    std::vector<std::unique_ptr<XPlugin>> plugins;
    plugins.push_back(std::move(plugin));
    engine.setPlugins(std::move(plugins));
    engine.startup();

    RtSnapshot* first = engine.snapshots().current();
    REQUIRE(first != nullptr);
    REQUIRE(first->generation() == 1);
    REQUIRE(dynamic_cast<const Marker*>(first->objects().at(0).get())->value == 1);

    engine.compile();
    RtSnapshot* second = engine.snapshots().current();
    REQUIRE(second->generation() == 2);
    REQUIRE(second != first);
    REQUIRE(dynamic_cast<const Marker*>(second->objects().at(0).get())->value == 2);
    REQUIRE(pluginPtr->compiles == 2);
    REQUIRE(dynamic_cast<const Marker*>(first->objects().at(0).get())->value == 1);
}

TEST_CASE("control clock ticks without an audio device") {
    struct Counter final : ControlClock::Target {
        std::atomic<int> ticks{0};
        void onTick(const TickInfo&) noexcept override { ticks.fetch_add(1, std::memory_order_relaxed); }
    } counter;
    ControlClock clock(counter);
    clock.start(0.02);
    std::this_thread::sleep_for(std::chrono::milliseconds(280));
    clock.stop();
    REQUIRE(counter.ticks.load() >= 5);
    REQUIRE(clock.ticks() == static_cast<std::uint64_t>(counter.ticks.load()));
}

TEST_CASE("real-time channel does not grow") {
    RtChannel<int, 4> channel;
    REQUIRE(channel.push(1));
    REQUIRE(channel.push(2));
    REQUIRE(channel.push(3));
    REQUIRE_FALSE(channel.push(4));
    int value = 0;
    REQUIRE(channel.pop(value));
    REQUIRE(value == 1);
    REQUIRE(channel.push(5));
    std::vector<int> got;
    while (channel.pop(value))
        got.push_back(value);
    REQUIRE(got == std::vector<int>{2, 3, 5});
}

TEST_CASE("project format constant") {
    Document document;
    REQUIRE(document.toJson({}).at("format") == kProjectFormat);
}
