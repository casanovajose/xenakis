#include "xen/Command.h"

#include <algorithm>
#include <stdexcept>

namespace xen {

void CommandStack::execute(std::unique_ptr<Command> command, CommandContext& context) {
    if (!command)
        throw std::runtime_error("command is empty");
    command->execute(context);
    undo_.push_back(std::move(command));
    redo_.clear();
}

bool CommandStack::undo(CommandContext& context) {
    if (undo_.empty())
        return false;
    auto command = std::move(undo_.back());
    undo_.pop_back();
    command->undo(context);
    redo_.push_back(std::move(command));
    return true;
}

bool CommandStack::redo(CommandContext& context) {
    if (redo_.empty())
        return false;
    auto command = std::move(redo_.back());
    redo_.pop_back();
    command->execute(context);
    undo_.push_back(std::move(command));
    return true;
}

void CommandStack::clear() {
    undo_.clear();
    redo_.clear();
}

void CommandRegistry::add(std::string name, std::string ownerId, Factory factory) {
    if (name.empty())
        throw std::runtime_error("command name is empty");
    if (entries_.count(name) != 0)
        throw std::runtime_error("command already registered " + name);
    entries_.emplace(std::move(name), Entry{std::move(ownerId), std::move(factory)});
}

bool CommandRegistry::contains(std::string_view name) const {
    return entries_.count(std::string(name)) != 0;
}

std::string CommandRegistry::ownerOf(std::string_view name) const {
    const auto it = entries_.find(std::string(name));
    if (it == entries_.end())
        throw std::runtime_error("unknown command " + std::string(name));
    return it->second.ownerId;
}

std::unique_ptr<Command> CommandRegistry::make(std::string_view name, const Json& args) const {
    const auto it = entries_.find(std::string(name));
    if (it == entries_.end())
        throw std::runtime_error("unknown command " + std::string(name));
    return it->second.factory(args);
}

} // namespace xen
