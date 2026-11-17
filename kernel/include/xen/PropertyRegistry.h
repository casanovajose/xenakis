#pragma once

#include "xen/Command.h"
#include "xen/Json.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xen {

enum class ValueType { Bool, Int, Float, String };

struct PropertyDesc {
    std::string id;
    std::string ownerId;
    std::string component;
    std::string field;
    ValueType type = ValueType::String;
    Json defaultValue;
    std::optional<double> min;
    std::optional<double> max;
    std::string oscName;
};

class PropertyRegistry {
public:
    void add(PropertyDesc desc);
    const PropertyDesc* find(std::string_view id) const;
    std::vector<PropertyDesc> forOwner(std::string_view ownerId) const;

private:
    std::vector<PropertyDesc> items_;
};

class SetPropertyCommand final : public Command {
public:
    SetPropertyCommand(const PropertyRegistry& properties, Json args);

    std::string_view name() const override { return "SetProperty"; }
    void execute(CommandContext& context) override;
    void undo(CommandContext& context) override;

private:
    void validate(const Json& value) const;
    void write(Document& document, const Json& value) const;

    PropertyDesc desc_;
    EntityId entity_;
    std::string propertyId_;
    Json newValue_;
    Json oldValue_;
    bool hadOld_ = false;
    bool captured_ = false;
};

}
