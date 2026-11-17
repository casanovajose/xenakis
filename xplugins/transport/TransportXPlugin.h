#pragma once

#include "xen/XPlugin.h"
#include "xen/XPluginRegistry.h"
#include "rt/TransportClock.h"

namespace xen::transport {

struct TransportReading {
    double seconds = 0;
    double beats = 0;
    double bpm = 120;
    double gridUnitSeconds = 0.5;
    int beatsPerBar = 4;
    bool loop = false;
    bool bpmMode = true;
    const char* state = "stopped";
};

class TransportXPlugin final : public XPlugin {
public:
    XPluginInfo info() const override;
    void contribute(Registry& registry) override;
    void activate(Document& document) override;
    void afterLoad(Document& document) override;
    void compile(const Document& document, RtBuilder& builder) override;
    void onCompiled(EventBus& events) override;
    void pump(EventBus& events) override;
    void migrate(Json& data, int fromVersion) override;

    TransportReading reading() const;
    std::string formatPosition() const;
    double beatsToSeconds(double beats) const;
    double secondsToBeats(double seconds) const;

    static const TransportXPlugin* find(const XPluginRegistry& plugins);

private:
    void ensure(Document& document) const;

    rt::TransportClock clock_;
    double publishedBpm_ = -1;
    bool publishedBpmMode_ = true;
    bool tempoDirty_ = false;
    double tempoBpm_ = 120;
    bool tempoBpmMode_ = true;
};

}
