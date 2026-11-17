#pragma once

#include "xen/Json.h"

#include <functional>
#include <string>
#include <vector>

namespace xen {

struct Event {
    std::string name;
    Json payload = Json::object();
    bool muted = false;
};

class EventBus {
public:
    using Handler = std::function<void(const Event&)>;

    int subscribe(std::string name, Handler handler);
    void unsubscribe(int token);
    void publish(Event event);

private:
    struct Subscription {
        int token = 0;
        std::string name;
        Handler handler;
    };

    std::vector<Subscription> subscriptions_;
    int nextToken_ = 1;
};

}
