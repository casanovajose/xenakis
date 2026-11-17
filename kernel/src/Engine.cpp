#include "xen/Engine.h"
#include "xen/Version.h"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace xen {

Engine::Engine() : clock_(*this) {}

Engine::~Engine() {
    stopClock();
    if (started_)
        shutdown();
    commandStack_.clear();
    snapshots_.reclaimAfterJoin();
}

void Engine::setPlugins(std::vector<std::unique_ptr<XPlugin>> plugins) {
    plugins_.setPlugins(std::move(plugins));
}

void Engine::startup() {
    if (started_)
        return;
    messageThread_ = std::this_thread::get_id();
    registry_ = std::make_unique<Registry>(properties_, commandRegistry_, components_);
    commandRegistry_.add("SetProperty", "kernel", [this](const Json& args) {
        return std::make_unique<SetPropertyCommand>(properties_, args);
    });
    plugins_.startup(*registry_, document_);
    started_ = true;
    compile();
}

void Engine::shutdown() {
    assertMessageThread();
    stopClock();
    plugins_.shutdown(document_);
    started_ = false;
}

void Engine::assertMessageThread() const {
    if (messageThread_ != std::thread::id() && std::this_thread::get_id() != messageThread_)
        throw std::logic_error("document work runs on the message thread");
}

CommandContext Engine::context() {
    return CommandContext{document_, events_};
}

void Engine::execute(std::string_view commandName, const Json& args) {
    assertMessageThread();
    if (!started_)
        throw std::runtime_error("engine has not been started");
    auto command = commandRegistry_.make(commandName, args);
    auto ctx = context();
    commandStack_.execute(std::move(command), ctx);
    compile();
}

bool Engine::undo() {
    assertMessageThread();
    auto ctx = context();
    const bool changed = commandStack_.undo(ctx);
    if (changed)
        compile();
    return changed;
}

bool Engine::redo() {
    assertMessageThread();
    auto ctx = context();
    const bool changed = commandStack_.redo(ctx);
    if (changed)
        compile();
    return changed;
}

void Engine::setProperty(const EntityId& entity, std::string_view propertyId, Json value) {
    execute("SetProperty", Json{{"entity", entity}, {"property", propertyId}, {"value", std::move(value)}});
}

void Engine::compile() {
    assertMessageThread();
    RtBuilder builder;
    plugins_.compileAll(document_, builder);
    plugins_.notifyCompiled(events_);
    snapshots_.publish(std::make_unique<RtSnapshot>(nextGeneration_++, builder.take()));
}

void Engine::pump() {
    assertMessageThread();
    plugins_.pump(events_);
}

void Engine::saveProject(const std::filesystem::path& path) const {
    assertMessageThread();
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("cannot write " + path.string());
    out << document_.toJson(plugins_.versions()).dump(2) << '\n';
}

void Engine::loadProject(const std::filesystem::path& path) {
    assertMessageThread();
    if (!started_)
        throw std::runtime_error("engine has not been started");
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot read " + path.string());
    Json project;
    in >> project;
    if (!project.is_object() || project.value("format", std::string()) != kProjectFormat)
        throw std::runtime_error("unsupported project format");

    if (project.contains("entities") && project["entities"].is_array()) {
        const Json versions = project.contains("xplugins") ? project["xplugins"] : Json::object();
        for (auto& entity : project["entities"]) {
            if (!entity.contains("components") || !entity["components"].is_object())
                continue;
            for (auto& [owner, blob] : entity["components"].items()) {
                XPlugin* plugin = plugins_.find(owner);
                if (plugin == nullptr || !versions.contains(owner) || !versions[owner].is_number_integer())
                    continue;
                const int fromVersion = versions[owner].get<int>();
                if (fromVersion < plugin->info().version)
                    plugin->migrate(blob, fromVersion);
            }
        }
    }

    document_.replaceFromJson(project);
    commandStack_.clear();
    plugins_.afterLoad(document_);
    compile();
}

void Engine::startClock(double periodSeconds) {
    assertMessageThread();
    clock_.start(periodSeconds);
}

void Engine::stopClock() {
    clock_.stop();
    snapshots_.reclaimAfterJoin();
}

void Engine::onTick(const TickInfo& tick) noexcept {
    RtSnapshot* snapshot = snapshots_.loadForTick();
    if (snapshot != nullptr)
        snapshot->tick(tick);
    snapshots_.acknowledge(snapshot != nullptr ? snapshot->generation() : 0);
}

} // namespace xen
