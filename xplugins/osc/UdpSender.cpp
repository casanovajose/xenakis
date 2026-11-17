#include "UdpSender.h"

#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace xen::osc {

struct UdpSender::Impl {
#if defined(_WIN32)
    SOCKET socket = INVALID_SOCKET;
    bool started = false;
#else
    int socket = -1;
#endif
};

UdpSender::UdpSender() : impl_(new Impl) {}

UdpSender::~UdpSender() {
    close();
    delete impl_;
}

bool UdpSender::open() {
#if defined(_WIN32)
    if (!impl_->started) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return false;
        impl_->started = true;
    }
    if (impl_->socket != INVALID_SOCKET)
        return true;
    impl_->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return impl_->socket != INVALID_SOCKET;
#else
    if (impl_->socket >= 0)
        return true;
    impl_->socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    return impl_->socket >= 0;
#endif
}

void UdpSender::close() {
#if defined(_WIN32)
    if (impl_->socket != INVALID_SOCKET) {
        ::closesocket(impl_->socket);
        impl_->socket = INVALID_SOCKET;
    }
    if (impl_->started) {
        WSACleanup();
        impl_->started = false;
    }
#else
    if (impl_->socket >= 0) {
        ::close(impl_->socket);
        impl_->socket = -1;
    }
#endif
}

bool UdpSender::send(const char* host, int port, const std::uint8_t* data, int size) {
    if (!open() || host == nullptr || data == nullptr || size <= 0 || port <= 0)
        return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1)
        return false;
#if defined(_WIN32)
    const int sent = ::sendto(impl_->socket, reinterpret_cast<const char*>(data), size, 0,
                              reinterpret_cast<sockaddr*>(&address), sizeof(address));
#else
    const int sent = static_cast<int>(::sendto(impl_->socket, data, static_cast<std::size_t>(size), 0,
                                               reinterpret_cast<sockaddr*>(&address), sizeof(address)));
#endif
    return sent == size;
}

}
