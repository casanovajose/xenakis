#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace xen::osc {

inline void appendBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

inline void pad4(std::vector<std::uint8_t>& out) {
    while (out.size() % 4 != 0)
        out.push_back(0);
}

inline void appendString(std::vector<std::uint8_t>& out, const char* text) {
    const std::size_t size = text == nullptr ? 0 : std::strlen(text);
    appendBytes(out, text == nullptr ? "" : text, size + 1);
    pad4(out);
}

inline void appendInt(std::vector<std::uint8_t>& out, std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>((raw >> 24) & 0xff),
        static_cast<std::uint8_t>((raw >> 16) & 0xff),
        static_cast<std::uint8_t>((raw >> 8) & 0xff),
        static_cast<std::uint8_t>(raw & 0xff),
    };
    appendBytes(out, bytes, 4);
}

inline void appendFloat(std::vector<std::uint8_t>& out, float value) {
    static_assert(sizeof(float) == 4, "float32");
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, 4);
    appendInt(out, static_cast<std::int32_t>(raw));
}

inline void appendDouble(std::vector<std::uint8_t>& out, double value) {
    static_assert(sizeof(double) == 8, "float64");
    std::uint64_t raw = 0;
    std::memcpy(&raw, &value, 8);
    const std::uint8_t bytes[8] = {
        static_cast<std::uint8_t>((raw >> 56) & 0xff), static_cast<std::uint8_t>((raw >> 48) & 0xff),
        static_cast<std::uint8_t>((raw >> 40) & 0xff), static_cast<std::uint8_t>((raw >> 32) & 0xff),
        static_cast<std::uint8_t>((raw >> 24) & 0xff), static_cast<std::uint8_t>((raw >> 16) & 0xff),
        static_cast<std::uint8_t>((raw >> 8) & 0xff),  static_cast<std::uint8_t>(raw & 0xff),
    };
    appendBytes(out, bytes, 8);
}

inline std::uint64_t ntpFromUnix(double unixSeconds) {
    const double ntp = unixSeconds + 2208988800.0;
    if (!(ntp > 0.0))
        return 0;
    const double whole = std::floor(ntp);
    const double fraction = ntp - whole;
    const auto seconds = static_cast<std::uint32_t>(whole);
    const auto frac = static_cast<std::uint32_t>(fraction * 4294967296.0);
    return (static_cast<std::uint64_t>(seconds) << 32) | frac;
}

inline double unixFromNtp(std::uint64_t tag) {
    const double seconds = static_cast<double>(tag >> 32);
    const double fraction = static_cast<double>(tag & 0xffffffffu) / 4294967296.0;
    return seconds + fraction - 2208988800.0;
}

struct Arg {
    char type = 's';
    char text[48]{};
    std::int32_t integer = 0;
    float number = 0;
    double time = 0;
};

struct Outgoing {
    char address[96]{};
    Arg args[8]{};
    int argc = 0;
    double unixSeconds = 0;
    bool bundle = true;
};

inline std::vector<std::uint8_t> encodeMessage(const Outgoing& message) {
    std::vector<std::uint8_t> out;
    appendString(out, message.address);
    std::string types = ",";
    for (int i = 0; i < message.argc; ++i)
        types.push_back(message.args[i].type);
    appendString(out, types.c_str());
    for (int i = 0; i < message.argc; ++i) {
        const Arg& arg = message.args[i];
        if (arg.type == 's')
            appendString(out, arg.text);
        else if (arg.type == 'i')
            appendInt(out, arg.integer);
        else if (arg.type == 'f')
            appendFloat(out, arg.number);
        else if (arg.type == 'd')
            appendDouble(out, arg.time);
    }
    return out;
}

inline std::vector<std::uint8_t> encodePacket(const Outgoing& message) {
    const std::vector<std::uint8_t> body = encodeMessage(message);
    if (!message.bundle)
        return body;
    std::vector<std::uint8_t> packet;
    appendString(packet, "#bundle");
    const std::uint64_t tag = ntpFromUnix(message.unixSeconds);
    const std::uint8_t bytes[8] = {
        static_cast<std::uint8_t>((tag >> 56) & 0xff), static_cast<std::uint8_t>((tag >> 48) & 0xff),
        static_cast<std::uint8_t>((tag >> 40) & 0xff), static_cast<std::uint8_t>((tag >> 32) & 0xff),
        static_cast<std::uint8_t>((tag >> 24) & 0xff), static_cast<std::uint8_t>((tag >> 16) & 0xff),
        static_cast<std::uint8_t>((tag >> 8) & 0xff),  static_cast<std::uint8_t>(tag & 0xff),
    };
    appendBytes(packet, bytes, 8);
    appendInt(packet, static_cast<std::int32_t>(body.size()));
    appendBytes(packet, body.data(), body.size());
    return packet;
}

}
