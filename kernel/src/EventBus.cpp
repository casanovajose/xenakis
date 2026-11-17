#include "xen/EventBus.h"

#include <algorithm>

namespace xen {

int EventBus::subscribe(std::string name, Handler handler) {
    const int token = nextToken_++;
    subscriptions_.push_back(Subscription{token, std::move(name), std::move(handler)});
    return token;
}

void EventBus::unsubscribe(int token) {
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [token](const Subscription& subscription) { return subscription.token == token; }),
        subscriptions_.end());
}

void EventBus::publish(Event event) {
    const auto snapshot = subscriptions_;
    for (const auto& subscription : snapshot) {
        if (subscription.name == event.name || subscription.name == "*")
            subscription.handler(event);
    }
}

} // namespace xen
