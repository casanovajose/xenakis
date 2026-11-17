#include "GroupsXPlugin.h"
#include "rt/GroupSnapshot.h"
#include "xen/Engine.h"
#include "xen/XPluginRegistration.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace xen;

namespace {

std::unique_ptr<Engine> started() {
    auto engine = std::make_unique<Engine>();
    engine->setPlugins(createXPlugins());
    engine->startup();
    return engine;
}

const Json& groupOf(const Document& document, const EntityId& id) {
    const Json* group = document.findComponent(id, "xen.groups", "Group");
    REQUIRE(group != nullptr);
    return *group;
}

std::vector<EntityId> scoreEntities(const Document& document) {
    std::vector<EntityId> ids;
    for (const auto& id : document.entities()) {
        if (id != "transport" && id != "osc")
            ids.push_back(id);
    }
    return ids;
}

} // namespace

TEST_CASE("groups create, nest, reorder, and undo") {
    auto engine = started();
    engine->execute("AddGroup", Json{{"name", "Intro"}});
    engine->execute("AddGroup", Json{{"name", "Water"}});
    engine->execute("AddGroup", Json{{"name", "Air"}});
    REQUIRE(scoreEntities(engine->document()) == std::vector<EntityId>{"G1", "G2", "G3"});

    engine->execute("MoveToGroup", Json{{"entity", "G2"}, {"group", "G1"}});
    engine->execute("MoveToGroup", Json{{"entity", "G3"}, {"group", "G1"}});
    REQUIRE(groupOf(engine->document(), "G1")["children"] == Json::array({"G2", "G3"}));

    engine->execute("Reorder", Json{{"group", "G1"}, {"children", Json::array({"G3", "G2"})}});
    REQUIRE(groupOf(engine->document(), "G1")["children"] == Json::array({"G3", "G2"}));
    REQUIRE(engine->undo());
    REQUIRE(groupOf(engine->document(), "G1")["children"] == Json::array({"G2", "G3"}));

    REQUIRE_THROWS(engine->execute("MoveToGroup", Json{{"entity", "G1"}, {"group", "G2"}}));
    REQUIRE(groupOf(engine->document(), "G1")["children"] == Json::array({"G2", "G3"}));
}

TEST_CASE("mute follows the parent group and stays in the snapshot") {
    auto engine = started();
    engine->execute("AddGroup", Json{{"name", "Intro"}});
    const auto child = engine->document().createEntity("E");
    engine->execute("MoveToGroup", Json{{"entity", child}, {"group", "G1"}});
    REQUIRE_FALSE(groups::effectiveMute(engine->document(), child));

    engine->setProperty("G1", "xen.groups.mute", true);
    REQUIRE(groups::effectiveMute(engine->document(), "G1"));
    REQUIRE(groups::effectiveMute(engine->document(), child));

    RtSnapshot* snapshot = engine->snapshots().current();
    REQUIRE(snapshot != nullptr);
    const groups::rt::GroupSnapshot* compiled = nullptr;
    for (const auto& object : snapshot->objects()) {
        if (const auto* group = dynamic_cast<const groups::rt::GroupSnapshot*>(object.get()))
            compiled = group;
    }
    REQUIRE(compiled != nullptr);
    REQUIRE(compiled->items().size() == 1);
    REQUIRE(compiled->items().at(0).mute);
    REQUIRE(compiled->items().at(0).children.size() == 1);
}

TEST_CASE("group ids are not reused after undo") {
    auto engine = started();
    engine->execute("AddGroup", Json{{"name", "A"}});
    engine->execute("AddGroup", Json{{"name", "B"}});
    REQUIRE(engine->undo());
    engine->execute("AddGroup", Json{{"name", "C"}});
    REQUIRE(scoreEntities(engine->document()) == std::vector<EntityId>{"G1", "G3"});
}
