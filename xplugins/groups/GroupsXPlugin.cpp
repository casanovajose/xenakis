#include "GroupsXPlugin.h"
#include "rt/GroupSnapshot.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace xen::groups {
namespace {

constexpr const char* kOwner = "xen.groups";

Json objectOrEmpty(const Json* value) {
    if (value != nullptr && value->is_object())
        return *value;
    return Json::object();
}

Json* childArray(Json& group) {
    if (!group.contains("children") || !group["children"].is_array())
        group["children"] = Json::array();
    return &group["children"];
}

void removeChild(Json& group, const std::string& child) {
    Json* children = childArray(group);
    for (auto it = children->begin(); it != children->end(); ++it) {
        if (it->is_string() && it->get<std::string>() == child) {
            children->erase(it);
            return;
        }
    }
}

void appendChild(Json& group, const std::string& child) {
    Json* children = childArray(group);
    for (const auto& item : *children) {
        if (item.is_string() && item.get<std::string>() == child)
            return;
    }
    children->push_back(child);
}

bool parentsReach(const Document& document, const EntityId& start, const EntityId& target) {
    std::string cursor = start;
    std::set<std::string> seen;
    while (seen.insert(cursor).second) {
        if (cursor == target)
            return true;
        const Json* member = document.findComponent(cursor, kOwner, "GroupMember");
        if (member == nullptr || !member->contains("parent") || !member->at("parent").is_string())
            return false;
        cursor = member->at("parent").get<std::string>();
    }
    return false;
}

class AddGroupCommand final : public Command {
public:
    explicit AddGroupCommand(Json args) {
        if (args.contains("name") && !args["name"].is_string())
            throw std::runtime_error("AddGroup name must be a string");
        nameText_ = args.value("name", std::string("Group"));
    }

    std::string_view name() const override { return "AddGroup"; }

    void execute(CommandContext& context) override {
        if (!created_) {
            id_ = context.document.createEntity("G");
            created_ = true;
        } else if (!context.document.contains(id_)) {
            context.document.insertEntity(id_);
        }
        context.document.setComponent(id_, kOwner, "Group", Json{
            {"name", nameText_},
            {"visible", true},
            {"mute", false},
            {"children", Json::array()},
        });
        context.events.publish(Event{"group.created", Json{{"id", id_}, {"name", nameText_}}, false});
    }

    void undo(CommandContext& context) override {
        context.document.destroyEntity(id_);
        context.events.publish(Event{"group.removed", Json{{"id", id_}}, false});
    }

private:
    std::string nameText_;
    EntityId id_;
    bool created_ = false;
};

class MoveToGroupCommand final : public Command {
public:
    explicit MoveToGroupCommand(Json args) {
        if (!args.contains("entity") || !args.contains("group"))
            throw std::runtime_error("MoveToGroup needs entity and group");
        entity_ = args.at("entity").get<std::string>();
        group_ = args.at("group").get<std::string>();
        if (entity_.empty() || group_.empty())
            throw std::runtime_error("MoveToGroup ids are empty");
        if (entity_ == group_)
            throw std::runtime_error("a group cannot contain itself");
    }

    std::string_view name() const override { return "MoveToGroup"; }

    void execute(CommandContext& context) override {
        if (!context.document.contains(entity_) || !context.document.contains(group_))
            throw std::runtime_error("MoveToGroup entity is missing");
        if (context.document.findComponent(group_, kOwner, "Group") == nullptr)
            throw std::runtime_error("MoveToGroup target is not a group");
        if (parentsReach(context.document, group_, entity_))
            throw std::runtime_error("MoveToGroup would cycle");

        if (!captured_) {
            capture(context.document);
            captured_ = true;
        }
        apply(context.document);
        context.events.publish(Event{"group.moved", Json{{"entity", entity_}, {"group", group_}}, false});
    }

    void undo(CommandContext& context) override {
        restore(context.document, entity_, memberExisted_, memberBlob_);
        restore(context.document, group_, groupExisted_, groupBlob_);
        if (!oldParent_.empty())
            restore(context.document, oldParent_, oldParentExisted_, oldParentBlob_);
    }

private:
    void capture(const Document& document) {
        const Json member = document.ownerBlob(entity_, kOwner);
        memberExisted_ = !member.is_null();
        memberBlob_ = memberExisted_ ? member : Json();
        if (memberExisted_ && member.contains("GroupMember") && member["GroupMember"].is_object()
            && member["GroupMember"].contains("parent"))
            oldParent_ = member["GroupMember"].value("parent", std::string());

        const Json groupBlob = document.ownerBlob(group_, kOwner);
        groupExisted_ = !groupBlob.is_null();
        groupBlob_ = groupBlob;
        if (!oldParent_.empty()) {
            const Json oldBlob = document.ownerBlob(oldParent_, kOwner);
            oldParentExisted_ = !oldBlob.is_null();
            oldParentBlob_ = oldBlob;
        }
    }

    void apply(Document& document) const {
        if (!oldParent_.empty() && oldParent_ != group_) {
            Json oldGroup = objectOrEmpty(document.findComponent(oldParent_, kOwner, "Group"));
            removeChild(oldGroup, entity_);
            document.setComponent(oldParent_, kOwner, "Group", std::move(oldGroup));
        }
        Json destination = objectOrEmpty(document.findComponent(group_, kOwner, "Group"));
        appendChild(destination, entity_);
        document.setComponent(group_, kOwner, "Group", std::move(destination));
        document.setComponent(entity_, kOwner, "GroupMember", Json{{"parent", group_}});
    }

    static void restore(Document& document, const EntityId& id, bool existed, const Json& blob) {
        if (!existed)
            document.eraseOwner(id, kOwner);
        else
            document.setOwnerBlob(id, kOwner, blob);
    }

    EntityId entity_;
    EntityId group_;
    bool captured_ = false;
    bool memberExisted_ = false;
    bool groupExisted_ = false;
    bool oldParentExisted_ = false;
    Json memberBlob_;
    Json groupBlob_;
    Json oldParentBlob_;
    std::string oldParent_;
};

class ReorderCommand final : public Command {
public:
    explicit ReorderCommand(Json args) {
        if (!args.contains("group") || !args.contains("children") || !args["children"].is_array())
            throw std::runtime_error("Reorder needs group and children");
        group_ = args.at("group").get<std::string>();
        for (const auto& child : args["children"]) {
            if (!child.is_string())
                throw std::runtime_error("Reorder children must be ids");
            children_.push_back(child.get<std::string>());
        }
    }

    std::string_view name() const override { return "Reorder"; }

    void execute(CommandContext& context) override {
        const Json* group = context.document.findComponent(group_, kOwner, "Group");
        if (group == nullptr)
            throw std::runtime_error("Reorder target is not a group");
        std::vector<std::string> current;
        if (group->contains("children") && group->at("children").is_array()) {
            for (const auto& child : group->at("children")) {
                if (child.is_string())
                    current.push_back(child.get<std::string>());
            }
        }
        if (!sameIds(current, children_))
            throw std::runtime_error("Reorder must keep the same children");
        if (!captured_) {
            blob_ = context.document.ownerBlob(group_, kOwner);
            captured_ = true;
        }
        Json updated = *group;
        updated["children"] = children_;
        context.document.setComponent(group_, kOwner, "Group", std::move(updated));
        context.events.publish(Event{"group.reordered", Json{{"group", group_}, {"children", children_}}, false});
    }

    void undo(CommandContext& context) override {
        context.document.setOwnerBlob(group_, kOwner, blob_);
    }

private:
    static bool sameIds(std::vector<std::string> left, std::vector<std::string> right) {
        std::sort(left.begin(), left.end());
        std::sort(right.begin(), right.end());
        return left == right;
    }

    EntityId group_;
    std::vector<std::string> children_;
    Json blob_;
    bool captured_ = false;
};

PropertyDesc property(std::string field, ValueType type, Json fallback) {
    return PropertyDesc{
        std::string(kOwner) + "." + field,
        kOwner,
        "Group",
        field,
        type,
        std::move(fallback),
        std::nullopt,
        std::nullopt,
        field,
    };
}

} // namespace

XPluginInfo GroupsXPlugin::info() const {
    return XPluginInfo{"xen.groups", "Groups", 1, {}, {}};
}

void GroupsXPlugin::contribute(Registry& registry) {
    registry.components().add(kOwner, "Group");
    registry.components().add(kOwner, "GroupMember");
    registry.properties().add(property("name", ValueType::String, ""));
    registry.properties().add(property("visible", ValueType::Bool, true));
    registry.properties().add(property("mute", ValueType::Bool, false));
    registry.commands().add("AddGroup", kOwner, [](const Json& args) {
        return std::make_unique<AddGroupCommand>(args);
    });
    registry.commands().add("MoveToGroup", kOwner, [](const Json& args) {
        return std::make_unique<MoveToGroupCommand>(args);
    });
    registry.commands().add("Reorder", kOwner, [](const Json& args) {
        return std::make_unique<ReorderCommand>(args);
    });
    registry.addEvent(kOwner, "group.created");
    registry.addEvent(kOwner, "group.removed");
    registry.addEvent(kOwner, "group.moved");
    registry.addEvent(kOwner, "group.reordered");
}

void GroupsXPlugin::compile(const Document& document, RtBuilder& builder) {
    std::vector<rt::GroupItem> items;
    for (const auto& id : document.entities()) {
        const Json* group = document.findComponent(id, kOwner, "Group");
        if (group == nullptr)
            continue;
        rt::GroupItem item;
        item.id = id;
        item.name = group->value("name", std::string());
        item.visible = group->value("visible", true);
        item.mute = group->value("mute", false);
        if (group->contains("children") && group->at("children").is_array()) {
            for (const auto& child : group->at("children")) {
                if (child.is_string())
                    item.children.push_back(child.get<std::string>());
            }
        }
        items.push_back(std::move(item));
    }
    builder.add(std::make_unique<rt::GroupSnapshot>(std::move(items)));
}

void GroupsXPlugin::migrate(Json& data, int fromVersion) {
    if (fromVersion >= 1 || !data.is_object() || !data.contains("Group") || !data["Group"].is_object())
        return;
    Json& group = data["Group"];
    if (!group.contains("name"))
        group["name"] = "";
    if (!group.contains("visible"))
        group["visible"] = true;
    if (!group.contains("mute"))
        group["mute"] = false;
    if (!group.contains("children"))
        group["children"] = Json::array();
}

bool effectiveMute(const Document& document, const EntityId& id) {
    std::string cursor = id;
    std::set<std::string> seen;
    while (seen.insert(cursor).second) {
        const Json* group = document.findComponent(cursor, kOwner, "Group");
        if (group != nullptr && group->value("mute", false))
            return true;
        const Json* member = document.findComponent(cursor, kOwner, "GroupMember");
        if (member == nullptr || !member->contains("parent") || !member->at("parent").is_string())
            return false;
        cursor = member->at("parent").get<std::string>();
    }
    return false;
}

} // namespace xen::groups
