#pragma once

#include <cstdint>

namespace xen::osc {

class UdpSender {
public:
    UdpSender();
    ~UdpSender();

    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    bool open();
    void close();
    bool send(const char* host, int port, const std::uint8_t* data, int size);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}
