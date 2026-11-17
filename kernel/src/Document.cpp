#include "xen/Document.h"
#include "xen/Version.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <stdexcept>

namespace xen {
namespace {

std::string makeProjectId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    constexpr char hex[] = "0123456789abcdef";
    std::string id(16, '0');
    for (char& c : id)
        c = hex[dist(gen)];
    return id;
}

} // namespace

Document::Document() : projectId_(makeProjectId()) {}

void Document::bump() { ++revision_; }

Document::Entity& Document::require(const EntityId& id) {
    const auto it = entities_.find(id);
    if (it == entities_.end())
        throw std::runtime_error("unknown entity " + id);
    return it->second;
}

const Document::Entity& Document::require(const EntityId& id) const {
    const auto it = entities_.find(id);
    if (it == entities_.end())
        throw std::runtime_error("unknown entity " + id);
    return it->second;
}

void Document::noteId(const EntityId& id) {
    std::size_t index = id.size();
    while (index > 0 && std::isdigit(static_cast<unsigned char>(id[index - 1])) != 0)
        --index;
    if (index == id.size() || index == 0)
        return;
    const auto prefix = id.substr(0, index);
    const int number = std::stoi(id.substr(index));
    idCounters_[prefix] = std::max(idCounters_[prefix], number);
}

EntityId Document::createEntity(std::string_view prefix) {
    if (prefix.empty())
        throw std::runtime_error("entity prefix is empty");
    auto& counter = idCounters_[std::string(prefix)];
    EntityId id;
    do {
        id = std::string(prefix) + std::to_string(++counter);
    } while (entities_.count(id) != 0);
    entities_.emplace(id, Entity{});
    order_.push_back(id);
    bump();
    return id;
}

void Document::insertEntity(const EntityId& id) {
    if (id.empty())
        throw std::runtime_error("entity id is empty");
    if (entities_.count(id) != 0)
        throw std::runtime_error("entity already exists " + id);
    entities_.emplace(id, Entity{});
    order_.push_back(id);
    noteId(id);
    bump();
}

void Document::destroyEntity(const EntityId& id) {
    if (entities_.erase(id) == 0)
        throw std::runtime_error("unknown entity " + id);
    order_.erase(std::remove(order_.begin(), order_.end(), id), order_.end());
    links_.erase(std::remove_if(links_.begin(), links_.end(), [&](const Link& link) {
        if (link.from == id)
            return true;
        return std::find(link.to.begin(), link.to.end(), id) != link.to.end();
    }), links_.end());
    bump();
}

bool Document::contains(const EntityId& id) const {
    return entities_.count(id) != 0;
}

std::vector<EntityId> Document::entities() const { return order_; }

void Document::setComponent(const EntityId& id, std::string_view ownerId, std::string_view type, Json value) {
    if (ownerId.empty() || type.empty())
        throw std::runtime_error("component owner and type are required");
    auto& blob = require(id).owners[std::string(ownerId)];
    if (!blob.is_object())
        blob = Json::object();
    blob[std::string(type)] = std::move(value);
    bump();
}

void Document::eraseOwner(const EntityId& id, std::string_view ownerId) {
    require(id).owners.erase(std::string(ownerId));
    bump();
}

void Document::setOwnerBlob(const EntityId& id, std::string_view ownerId, Json blob) {
    if (ownerId.empty())
        throw std::runtime_error("component owner is required");
    require(id).owners[std::string(ownerId)] = std::move(blob);
    bump();
}

const Json* Document::findComponent(const EntityId& id, std::string_view ownerId, std::string_view type) const {
    const auto entity = entities_.find(id);
    if (entity == entities_.end())
        return nullptr;
    const auto owner = entity->second.owners.find(std::string(ownerId));
    if (owner == entity->second.owners.end() || !owner->second.is_object())
        return nullptr;
    const auto component = owner->second.find(std::string(type));
    if (component == owner->second.end())
        return nullptr;
    return &(*component);
}

Json Document::ownerBlob(const EntityId& id, std::string_view ownerId) const {
    const auto entity = entities_.find(id);
    if (entity == entities_.end())
        return Json();
    const auto owner = entity->second.owners.find(std::string(ownerId));
    if (owner == entity->second.owners.end())
        return Json();
    return owner->second;
}

void Document::addLink(Link link) {
    if (link.type.empty())
        throw std::runtime_error("link type is required");
    if (!contains(link.from))
        throw std::runtime_error("link source does not exist");
    for (const auto& target : link.to) {
        if (!contains(target))
            throw std::runtime_error("link target does not exist");
    }
    links_.push_back(std::move(link));
    bump();
}

Json Document::toJson(const std::map<std::string, int>& xpluginVersions) const {
    Json entities = Json::array();
    for (const auto& id : order_) {
        Json components = Json::object();
        for (const auto& [owner, blob] : entities_.at(id).owners)
            components[owner] = blob;
        entities.push_back(Json{{"id", id}, {"components", std::move(components)}});
    }

    Json links = Json::array();
    for (const auto& link : links_)
        links.push_back(Json{{"type", link.type}, {"from", link.from}, {"to", link.to}});

    Json versions = Json::object();
    for (const auto& [id, version] : xpluginVersions)
        versions[id] = version;

    Json counters = Json::object();
    for (const auto& [prefix, number] : idCounters_)
        counters[prefix] = number;

    return Json{
        {"format", kProjectFormat},
        {"id", projectId_},
        {"xplugins", std::move(versions)},
        {"ids", std::move(counters)},
        {"entities", std::move(entities)},
        {"links", std::move(links)},
    };
}

void Document::replaceFromJson(const Json& project) {
    if (!project.is_object())
        throw std::runtime_error("project JSON must be an object");
    if (project.value("format", std::string()) != kProjectFormat)
        throw std::runtime_error("unsupported project format");

    entities_.clear();
    order_.clear();
    links_.clear();
    idCounters_.clear();

    projectId_ = project.value("id", makeProjectId());

    if (project.contains("ids") && project["ids"].is_object()) {
        for (const auto& [prefix, number] : project["ids"].items()) {
            if (number.is_number_integer())
                idCounters_[prefix] = number.get<int>();
        }
    }

    if (project.contains("entities") && project["entities"].is_array()) {
        for (const auto& entityJson : project["entities"]) {
            if (!entityJson.is_object() || !entityJson.contains("id"))
                throw std::runtime_error("entity is missing an id");
            const auto id = entityJson["id"].get<std::string>();
            if (entities_.count(id) != 0)
                throw std::runtime_error("duplicate entity " + id);
            Entity entity;
            if (entityJson.contains("components") && entityJson["components"].is_object()) {
                for (const auto& [owner, blob] : entityJson["components"].items())
                    entity.owners.emplace(owner, blob);
            }
            entities_.emplace(id, std::move(entity));
            order_.push_back(id);
            noteId(id);
        }
    }

    if (project.contains("links") && project["links"].is_array()) {
        for (const auto& linkJson : project["links"]) {
            Link link;
            link.type = linkJson.value("type", std::string());
            link.from = linkJson.value("from", std::string());
            if (linkJson.contains("to") && linkJson["to"].is_array()) {
                for (const auto& target : linkJson["to"]) {
                    if (target.is_string())
                        link.to.push_back(target.get<std::string>());
                }
            }
            if (link.type.empty() || !contains(link.from))
                throw std::runtime_error("project link is invalid");
            for (const auto& target : link.to) {
                if (!contains(target))
                    throw std::runtime_error("project link target is missing");
            }
            links_.push_back(std::move(link));
        }
    }

    bump();
}

} // namespace xen
