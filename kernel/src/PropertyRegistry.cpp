#include "xen/PropertyRegistry.h"

#include <stdexcept>

namespace xen {
namespace {

void publishChange(EventBus& events, const EntityId& entity, const std::string& propertyId, const Json& value) {
    events.publish(Event{
        "property.changed",
        Json{{"entity", entity}, {"property", propertyId}, {"value", value}},
        false,
    });
}

} // namespace

void PropertyRegistry::add(PropertyDesc desc) {
    if (desc.id.empty() || desc.ownerId.empty() || desc.component.empty() || desc.field.empty())
        throw std::runtime_error("property descriptor is incomplete");
    if (find(desc.id) != nullptr)
        throw std::runtime_error("property already registered " + desc.id);
    items_.push_back(std::move(desc));
}

const PropertyDesc* PropertyRegistry::find(std::string_view id) const {
    for (const auto& item : items_) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

std::vector<PropertyDesc> PropertyRegistry::forOwner(std::string_view ownerId) const {
    std::vector<PropertyDesc> found;
    for (const auto& item : items_) {
        if (item.ownerId == ownerId)
            found.push_back(item);
    }
    return found;
}

SetPropertyCommand::SetPropertyCommand(const PropertyRegistry& properties, Json args) {
    if (!args.contains("entity") || !args.contains("property") || !args.contains("value"))
        throw std::runtime_error("SetProperty needs entity, property, and value");
    entity_ = args.at("entity").get<std::string>();
    propertyId_ = args.at("property").get<std::string>();
    newValue_ = args.at("value");
    const PropertyDesc* desc = properties.find(propertyId_);
    if (desc == nullptr)
        throw std::runtime_error("unknown property " + propertyId_);
    desc_ = *desc;
    validate(newValue_);
}

void SetPropertyCommand::validate(const Json& value) const {
    switch (desc_.type) {
    case ValueType::Bool:
        if (!value.is_boolean())
            throw std::runtime_error(desc_.id + " expects a bool");
        break;
    case ValueType::Int:
        if (!value.is_number_integer())
            throw std::runtime_error(desc_.id + " expects an int");
        if (desc_.min && value.get<double>() < *desc_.min)
            throw std::runtime_error(desc_.id + " is below its minimum");
        if (desc_.max && value.get<double>() > *desc_.max)
            throw std::runtime_error(desc_.id + " is above its maximum");
        break;
    case ValueType::Float:
        if (!value.is_number())
            throw std::runtime_error(desc_.id + " expects a number");
        if (desc_.min && value.get<double>() < *desc_.min)
            throw std::runtime_error(desc_.id + " is below its minimum");
        if (desc_.max && value.get<double>() > *desc_.max)
            throw std::runtime_error(desc_.id + " is above its maximum");
        break;
    case ValueType::String:
        if (!value.is_string())
            throw std::runtime_error(desc_.id + " expects a string");
        break;
    }
}

void SetPropertyCommand::write(Document& document, const Json& value) const {
    const Json* component = document.findComponent(entity_, desc_.ownerId, desc_.component);
    if (component == nullptr || !component->is_object())
        throw std::runtime_error("entity has no " + desc_.component + " component");
    Json updated = *component;
    updated[desc_.field] = value;
    document.setComponent(entity_, desc_.ownerId, desc_.component, std::move(updated));
}

void SetPropertyCommand::execute(CommandContext& context) {
    const Json* component = context.document.findComponent(entity_, desc_.ownerId, desc_.component);
    if (component == nullptr || !component->is_object())
        throw std::runtime_error("entity has no " + desc_.component + " component");
    if (!captured_) {
        if (component->contains(desc_.field)) {
            hadOld_ = true;
            oldValue_ = component->at(desc_.field);
        }
        captured_ = true;
    }
    write(context.document, newValue_);
    publishChange(context.events, entity_, propertyId_, newValue_);
}

void SetPropertyCommand::undo(CommandContext& context) {
    if (!hadOld_) {
        const Json* component = context.document.findComponent(entity_, desc_.ownerId, desc_.component);
        if (component == nullptr)
            throw std::runtime_error("entity has no " + desc_.component + " component");
        Json updated = *component;
        updated.erase(desc_.field);
        context.document.setComponent(entity_, desc_.ownerId, desc_.component, std::move(updated));
        publishChange(context.events, entity_, propertyId_, Json());
        return;
    }
    write(context.document, oldValue_);
    publishChange(context.events, entity_, propertyId_, oldValue_);
}

} // namespace xen
