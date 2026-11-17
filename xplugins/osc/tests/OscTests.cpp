#include "OscPacket.h"
#include "xen/Engine.h"
#include "xen/XPluginRegistration.h"
#include "TransportXPlugin.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

using namespace xen;

namespace {

struct Packet {
    double unixTime = 0;
    std::string address;
    std::string types;
    std::string cursor;
    std::string path;
    int dir = 0;
    int pass = 0;
    double time = 0;
    std::string dump;
};

std::uint32_t readInt(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]);
}

std::string readString(const std::uint8_t* data, int size, int& cursor) {
    if (cursor >= size)
        return {};
    const char* text = reinterpret_cast<const char*>(data + cursor);
    const int length = static_cast<int>(std::strlen(text));
    cursor += length + 1;
    while (cursor % 4 != 0)
        ++cursor;
    return text;
}

Packet parseMessage(const std::uint8_t* data, int size, double unixTime) {
    Packet packet;
    packet.unixTime = unixTime;
    int cursor = 0;
    packet.address = readString(data, size, cursor);
    const std::string types = readString(data, size, cursor);
    packet.types = types.size() > 0 && types[0] == ',' ? types.substr(1) : types;
    std::string dump = packet.address + " " + packet.types;
    for (char type : packet.types) {
        if (cursor + 4 > size && type != 's')
            break;
        if (type == 's') {
            const std::string text = readString(data, size, cursor);
            if (packet.cursor.empty())
                packet.cursor = text;
            else if (packet.path.empty())
                packet.path = text;
            dump += " \"" + text + "\"";
        } else if (type == 'i') {
            const int value = static_cast<int>(readInt(data + cursor));
            cursor += 4;
            if (packet.dir == 0)
                packet.dir = value;
            else
                packet.pass = value;
            dump += " " + std::to_string(value);
        } else if (type == 'f') {
            const std::uint32_t raw = readInt(data + cursor);
            cursor += 4;
            float value = 0;
            std::memcpy(&value, &raw, 4);
            dump += " " + std::to_string(value);
        } else if (type == 'd') {
            std::uint64_t raw = 0;
            for (int i = 0; i < 8; ++i)
                raw = (raw << 8) | data[cursor++];
            std::memcpy(&packet.time, &raw, 8);
            dump += " " + std::to_string(packet.time);
        }
    }
    packet.dump = dump;
    return packet;
}

std::vector<Packet> parseDatagram(const std::uint8_t* data, int size) {
    std::vector<Packet> packets;
    if (size >= 16 && std::strncmp(reinterpret_cast<const char*>(data), "#bundle", 7) == 0) {
        std::uint64_t tag = 0;
        for (int i = 8; i < 16; ++i)
            tag = (tag << 8) | data[i];
        const double unixTime = osc::unixFromNtp(tag);
        int cursor = 16;
        while (cursor + 4 <= size) {
            const int element = readInt(data + cursor);
            cursor += 4;
            if (element <= 0 || cursor + element > size)
                break;
            packets.push_back(parseMessage(data + cursor, element, unixTime));
            cursor += element;
        }
        return packets;
    }
    packets.push_back(parseMessage(data, size, 0));
    return packets;
}

class Receiver {
public:
    Receiver() {
#if defined(_WIN32)
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        ::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        socklen_t length = sizeof(address);
        ::getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &length);
        port_ = ntohs(address.sin_port);
#if defined(_WIN32)
        u_long mode = 1;
        ioctlsocket(socket_, FIONBIO, &mode);
#else
        // Non-blocking is unnecessary; tests use a short timeout via MSG and sleep.
#endif
    }

    ~Receiver() {
#if defined(_WIN32)
        if (socket_ != INVALID_SOCKET)
            closesocket(socket_);
        WSACleanup();
#else
        if (socket_ >= 0)
            ::close(socket_);
#endif
    }

    int port() const { return port_; }

    std::vector<Packet> poll() {
        std::vector<Packet> packets;
        for (;;) {
            std::uint8_t buffer[2048];
#if defined(_WIN32)
            const int size = ::recv(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
#else
            const int size = static_cast<int>(::recv(socket_, buffer, sizeof(buffer), 0));
#endif
            if (size <= 0)
                break;
            const auto parsed = parseDatagram(buffer, size);
            packets.insert(packets.end(), parsed.begin(), parsed.end());
        }
        return packets;
    }

private:
#if defined(_WIN32)
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
    int port_ = 0;
};

std::unique_ptr<Engine> started() {
    auto engine = std::make_unique<Engine>();
    engine->setPlugins(createXPlugins());
    engine->startup();
    return engine;
}

void addFastLine(Engine& engine, double speed) {
    engine.execute("AddGroup", Json{{"name", "Intro"}});
    engine.execute("AddPath", Json{{"name", "Line"}, {"points", Json::array({Json{{"x", 0.0}, {"y", 0.0}}, Json{{"x", 8.0}, {"y", 0.0}}})}});
    engine.execute("MoveToGroup", Json{{"entity", "P1"}, {"group", "G1"}});
    engine.execute("AddCursor", Json{{"path", "P1"}});
    engine.execute("AddTrigger", Json{{"path", "P1"}, {"u", 0.5}});
    engine.setProperty("C1", "xen.paths.Cursor.timingSpeed", speed);
    engine.setProperty("C1", "xen.paths.Cursor.pattern", "1 0");
}

} // namespace

TEST_CASE("trigger bundles match the oscdump shape and the crossing time") {
    Receiver receiver;
    auto engine = started();
    engine->execute("osc.setDestination", Json{{"host", "127.0.0.1"}, {"port", receiver.port()}});
    addFastLine(*engine, 40.0);
    const auto& transport = *transport::TransportXPlugin::find(engine->plugins());
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    const double transport0 = transport.reading().seconds;
    const double unix0 = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<Packet> packets;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < deadline) {
        engine->pump();
        const auto more = receiver.poll();
        packets.insert(packets.end(), more.begin(), more.end());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    engine->execute("transport.stop", Json::object());
    engine->stopClock();

    const Packet* trigger = nullptr;
    for (const auto& packet : packets) {
        if (packet.address == "/xen/trigger/T1")
            trigger = &packet;
    }
    REQUIRE(trigger != nullptr);
    std::cout << "oscdump " << trigger->dump << '\n';
    REQUIRE(trigger->types == "ssiifffd");
    REQUIRE(trigger->cursor == "C1");
    REQUIRE(trigger->path == "P1");
    REQUIRE(trigger->dir == 1);
    REQUIRE(trigger->time == Catch::Approx(0.05).margin(0.001));
    const double expected = unix0 + (trigger->time - transport0) + 0.010;
    REQUIRE(std::abs(trigger->unixTime - expected) < 0.001);
}

TEST_CASE("a thousand triggers a second do not block the clock") {
    Receiver receiver;
    auto engine = started();
    engine->execute("osc.setDestination", Json{{"host", "127.0.0.1"}, {"port", receiver.port()}});
    addFastLine(*engine, 4000.0);
    engine->setProperty("C1", "xen.paths.Cursor.pattern", "1");
    const auto& transport = *transport::TransportXPlugin::find(engine->plugins());
    engine->startClock(0.005);
    engine->execute("transport.play", Json::object());
    const auto wall0 = std::chrono::steady_clock::now();
    const double start = transport.reading().seconds;
    std::vector<Packet> packets;
    const auto deadline = wall0 + std::chrono::milliseconds(1000);
    while (std::chrono::steady_clock::now() < deadline) {
        engine->pump();
        const auto more = receiver.poll();
        packets.insert(packets.end(), more.begin(), more.end());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    engine->pump();
    const auto tail = receiver.poll();
    packets.insert(packets.end(), tail.begin(), tail.end());
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
    const double advanced = transport.reading().seconds - start;
    engine->execute("transport.stop", Json::object());
    engine->stopClock();
    int triggers = 0;
    for (const auto& packet : packets) {
        if (packet.address.rfind("/xen/trigger/", 0) == 0)
            ++triggers;
    }
    std::cout << "burst_triggers=" << triggers << " advanced=" << advanced << " wall=" << wall << '\n';
    REQUIRE(std::abs(advanced - wall) < 0.01);
    REQUIRE(triggers >= 900);
}
