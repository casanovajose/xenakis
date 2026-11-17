#pragma once

#include "xen/EventBus.h"
#include "xen/XPlugin.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xen {

class XPluginRegistry {
public:
    void setPlugins(std::vector<std::unique_ptr<XPlugin>> plugins);
    void startup(Registry& registry, Document& document);
    void shutdown(Document& document);
    void afterLoad(Document& document);
    void notifyCompiled(EventBus& events);
    void pump(EventBus& events);

    XPlugin* find(std::string_view id);
    const XPlugin* find(std::string_view id) const;

    const std::vector<std::string>& order() const { return order_; }
    std::map<std::string, int> versions() const;
    void compileAll(const Document& document, RtBuilder& builder);

private:
    std::vector<std::string> resolveOrder() const;

    std::vector<std::unique_ptr<XPlugin>> plugins_;
    std::vector<std::string> order_;
    bool started_ = false;
};

}
