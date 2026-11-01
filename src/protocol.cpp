#include "et08/protocol.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace et08 {
std::uint16_t switch_raw(Switch position) {
    switch (position) {
    case Switch::Up: return raw_min;
    case Switch::Middle: return raw_center;
    case Switch::Down: return raw_max;
    }
    throw std::invalid_argument("Invalid switch position");
}

Channels channels(const State& state) {
    Channels values{};
    values.fill(raw_center);
    // The reference driver treats SA == Middle && SD == Middle as offline.
    if (state.receiver_offline) return values;
    const auto raw = [](int axis) {
        return static_cast<std::uint16_t>(std::clamp(axis, axis_min, axis_max) + raw_center);
    };
    values[0] = raw(state.right_x);
    values[1] = raw(state.right_y);
    values[2] = raw(state.left_y);
    values[3] = raw(state.left_x);
    if (state.sa == Switch::Middle || state.sd == Switch::Middle)
        throw std::invalid_argument("SA and SD only have Up/Down positions");
    values[4] = switch_raw(state.sa);
    values[5] = switch_raw(state.sb);
    values[6] = switch_raw(state.sc);
    values[7] = switch_raw(state.sd);
    return values;
}

Frame pack(const Channels& values, bool frame_lost, bool failsafe) {
    Frame frame{};
    frame[0] = 0x0f;
    for (std::size_t channel = 0; channel < values.size(); ++channel) {
        if (values[channel] > 0x7ff) throw std::out_of_range("SBUS channel exceeds 11 bits");
        for (unsigned bit = 0; bit < 11; ++bit) {
            const auto offset = channel * 11 + bit;
            if (values[channel] & (1u << bit))
                frame[1 + offset / 8] |= static_cast<std::uint8_t>(1u << (offset % 8));
        }
    }
    frame[23] = static_cast<std::uint8_t>((frame_lost ? 0x04 : 0) | (failsafe ? 0x08 : 0));
    frame[24] = 0x00;
    return frame;
}

Frame encode(const State& state) { return pack(channels(state), state.frame_lost, state.failsafe); }

std::string hex(const Frame& frame) {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < frame.size(); ++i) {
        if (i) out << ' ';
        out << std::setw(2) << static_cast<unsigned>(frame[i]);
    }
    return out.str();
}
} // namespace et08
