#pragma once

#include "OscPacket.h"
#include "UdpSender.h"
#include "xen/RtChannel.h"
#include "xen/XPlugin.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace xen::osc {

struct SentNote {
    char text[160]{};
};

class OscXPlugin final : public XPlugin {
public:
    OscXPlugin();
    ~OscXPlugin() override;

    XPluginInfo info() const override;
    void contribute(Registry& registry) override;
    void activate(Document& document) override;
    void deactivate(Document& document) override;
    void afterLoad(Document& document) override;
    void compile(const Document& document, RtBuilder& builder) override;
    void onCompiled(EventBus& events) override;
    void pump(EventBus& events) override;
    void migrate(Json& data, int fromVersion) override;
    void sendTest();
    void panic();

    std::uint64_t dropped() const { return dropped_.load(); }

private:
    void ensure(Document& document);
    void refresh(const Document& document);
    void subscribe(EventBus& events);
    void enqueue(Outgoing message);
    void onTrigger(const Event& event);
    void onCursor(const char* suffix, const Event& event, bool withPass);
    void onTransport(const char* state, const Event& event);
    void threadMain();
    void copySettings(Outgoing& message) const;

    Document* document_ = nullptr;
    EventBus* events_ = nullptr;
    int tokens_[8]{};
    int tokenCount_ = 0;
    bool subscribed_ = false;

    mutable std::mutex settingsLock_;
    char prefix_[32] = "/xen";
    char hosts_[4][48]{};
    int ports_[4]{};
    bool enabled_[4]{};
    int destinationCount_ = 0;
    double latency_ = 0.01;
    bool bundles_ = true;
    int streamHz_ = 30;
    bool streamHalted_ = false;

    xen::RtChannel<Outgoing, 1024> outgoing_;
    xen::RtChannel<SentNote, 256> sent_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> dropped_{0};
    std::thread thread_;
};

}
