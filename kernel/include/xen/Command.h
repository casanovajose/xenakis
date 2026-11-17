#pragma once

#include "xen/Document.h"
#include "xen/EventBus.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xen {

struct CommandContext {
    Document& document;
    EventBus& events;
};

class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view name() const = 0;
    virtual void execute(CommandContext& context) = 0;
    virtual void undo(CommandContext& context) = 0;
};

class CommandStack {
public:
    void execute(std::unique_ptr<Command> command, CommandContext& context);
    bool undo(CommandContext& context);
    bool redo(CommandContext& context);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void clear();

private:
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
};

class CommandRegistry {
public:
    using Factory = std::function<std::unique_ptr<Command>(const Json& args)>;

    void add(std::string name, std::string ownerId, Factory factory);
    bool contains(std::string_view name) const;
    std::string ownerOf(std::string_view name) const;
    std::unique_ptr<Command> make(std::string_view name, const Json& args) const;

private:
    struct Entry {
        std::string ownerId;
        Factory factory;
    };

    std::map<std::string, Entry> entries_;
};

}
