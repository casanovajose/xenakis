#include "xen/XPluginRegistry.h"

#include <queue>
#include <stdexcept>

namespace xen {

void XPluginRegistry::setPlugins(std::vector<std::unique_ptr<XPlugin>> plugins) {
    if (started_)
        throw std::runtime_error("xPlugins cannot be replaced after startup");
    plugins_ = std::move(plugins);
}

void XPluginRegistry::startup(Registry& registry, Document& document) {
    if (started_)
        return;
    order_ = resolveOrder();
    for (const auto& id : order_)
        find(id)->contribute(registry);
    for (const auto& id : order_)
        find(id)->activate(document);
    started_ = true;
}

void XPluginRegistry::shutdown(Document& document) {
    if (!started_)
        return;
    for (auto it = order_.rbegin(); it != order_.rend(); ++it)
        find(*it)->deactivate(document);
    started_ = false;
}

XPlugin* XPluginRegistry::find(std::string_view id) {
    for (const auto& plugin : plugins_) {
        if (plugin->info().id == id)
            return plugin.get();
    }
    return nullptr;
}

const XPlugin* XPluginRegistry::find(std::string_view id) const {
    for (const auto& plugin : plugins_) {
        if (plugin->info().id == id)
            return plugin.get();
    }
    return nullptr;
}

std::map<std::string, int> XPluginRegistry::versions() const {
    std::map<std::string, int> versions;
    for (const auto& id : order_)
        versions.emplace(id, find(id)->info().version);
    return versions;
}

void XPluginRegistry::compileAll(const Document& document, RtBuilder& builder) {
    for (const auto& id : order_)
        find(id)->compile(document, builder);
}

std::vector<std::string> XPluginRegistry::resolveOrder() const {
    std::map<std::string, XPlugin*> byId;
    for (const auto& plugin : plugins_) {
        const auto info = plugin->info();
        if (info.id.empty())
            throw std::runtime_error("xPlugin id is empty");
        if (byId.count(info.id) != 0)
            throw std::runtime_error("duplicate xPlugin " + info.id);
        byId.emplace(info.id, plugin.get());
    }

    for (const auto& [id, plugin] : byId) {
        for (const auto& dependency : plugin->info().required) {
            if (byId.count(dependency.id) == 0)
                throw std::runtime_error(id + " requires " + dependency.id + ", which is not registered");
        }
    }

    std::map<std::string, std::vector<std::string>> outgoing;
    std::map<std::string, int> indegree;
    for (const auto& [id, plugin] : byId)
        indegree[id] = 0;

    const auto addEdge = [&](const std::string& from, const std::string& to) {
        outgoing[from].push_back(to);
        indegree[to] += 1;
    };

    for (const auto& [id, plugin] : byId) {
        const auto info = plugin->info();
        for (const auto& dependency : info.required)
            addEdge(dependency.id, id);
        for (const auto& enhanced : info.enhances) {
            if (byId.count(enhanced.id) != 0)
                addEdge(enhanced.id, id);
        }
    }

    std::queue<std::string> ready;
    for (const auto& [id, degree] : indegree) {
        if (degree == 0)
            ready.push(id);
    }

    std::vector<std::string> order;
    while (!ready.empty()) {
        const auto id = ready.front();
        ready.pop();
        order.push_back(id);
        for (const auto& next : outgoing[id]) {
            if (--indegree[next] == 0)
                ready.push(next);
        }
    }

    if (order.size() != byId.size())
        throw std::runtime_error("xPlugin dependency cycle");
    return order;
}

} // namespace xen
