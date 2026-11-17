#pragma once

#include "xen/ControlClock.h"
#include "xen/UiSlots.h"
#include "xen/XPluginRegistry.h"

#include <filesystem>
#include <thread>

namespace xen {

class Engine final : public ControlClock::Target {
public:
    Engine();
    ~Engine() override;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void setPlugins(std::vector<std::unique_ptr<XPlugin>> plugins);
    void startup();
    void shutdown();

    Document& document() { return document_; }
    const Document& document() const { return document_; }
    CommandStack& commands() { return commandStack_; }
    EventBus& events() { return events_; }
    PropertyRegistry& properties() { return properties_; }
    const XPluginRegistry& plugins() const { return plugins_; }
    UiRegistry& ui() { return ui_; }
    SnapshotStore& snapshots() { return snapshots_; }

    void execute(std::string_view commandName, const Json& args);
    bool undo();
    bool redo();
    void setProperty(const EntityId& entity, std::string_view propertyId, Json value);

    void compile();
    void pump();
    void saveProject(const std::filesystem::path& path) const;
    void loadProject(const std::filesystem::path& path);

    void startClock(double periodSeconds);
    void stopClock();

    void onTick(const TickInfo& tick) noexcept override;

private:
    void assertMessageThread() const;
    CommandContext context();

    Document document_;
    EventBus events_;
    CommandStack commandStack_;
    CommandRegistry commandRegistry_;
    PropertyRegistry properties_;
    ComponentRegistry components_;
    std::unique_ptr<Registry> registry_;
    XPluginRegistry plugins_;
    UiRegistry ui_;
    SnapshotStore snapshots_;
    ControlClock clock_;
    std::uint64_t nextGeneration_ = 1;
    bool started_ = false;
    std::thread::id messageThread_{};
};

}
