#pragma once

#include "xen/Json.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace xen {

using EntityId = std::string;

struct Link {
    std::string type;
    EntityId from;
    std::vector<EntityId> to;
};

// Message-thread document. The control clock never touches it.
// Components are JSON blobs keyed by owning xPlugin id, then component type.
// Blobs the running process does not understand are kept and written back.
class Document {
public:
    Document();

    EntityId createEntity(std::string_view prefix);
    void insertEntity(const EntityId& id);
    void destroyEntity(const EntityId& id);
    bool contains(const EntityId& id) const;
    std::vector<EntityId> entities() const;

    void setComponent(const EntityId& id, std::string_view ownerId, std::string_view type, Json value);
    void setOwnerBlob(const EntityId& id, std::string_view ownerId, Json blob);
    void eraseOwner(const EntityId& id, std::string_view ownerId);
    const Json* findComponent(const EntityId& id, std::string_view ownerId, std::string_view type) const;
    Json ownerBlob(const EntityId& id, std::string_view ownerId) const;

    void addLink(Link link);
    const std::vector<Link>& links() const { return links_; }

    const std::string& projectId() const { return projectId_; }
    std::uint64_t revision() const { return revision_; }

    Json toJson(const std::map<std::string, int>& xpluginVersions) const;
    void replaceFromJson(const Json& project);

private:
    struct Entity {
        std::map<std::string, Json> owners;
    };

    Entity& require(const EntityId& id);
    const Entity& require(const EntityId& id) const;
    void noteId(const EntityId& id);
    void bump();

    std::string projectId_;
    std::map<EntityId, Entity> entities_;
    std::vector<EntityId> order_;
    std::vector<Link> links_;
    std::map<std::string, int> idCounters_;
    std::uint64_t revision_ = 0;
};

}
