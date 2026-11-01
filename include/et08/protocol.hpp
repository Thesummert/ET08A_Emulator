#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace et08 {
inline constexpr int raw_min = 353;
inline constexpr int raw_center = 1024;
inline constexpr int raw_max = 1694;
inline constexpr int axis_min = raw_min - raw_center; // -671
inline constexpr int axis_max = raw_max - raw_center; // +670
inline constexpr int default_hz = 70;
using Channels = std::array<std::uint16_t, 16>;
using Frame = std::array<std::uint8_t, 25>;

enum class Switch { Up = 1, Down = 2, Middle = 3 };

struct State {
    int right_x = 0;
    int right_y = 0;
    int left_y = 0;
    int left_x = 0;
    Switch sa = Switch::Up;
    Switch sb = Switch::Up;
    Switch sc = Switch::Up;
    Switch sd = Switch::Up;
    bool receiver_offline = false;
    bool frame_lost = false;
    bool failsafe = false;
};

std::uint16_t switch_raw(Switch position);
Channels channels(const State& state);
// All 16 channels are packed as consecutive little-endian 11-bit values.
// Throws for out-of-range raw channels, so invalid values never corrupt neighbors.
Frame pack(const Channels& values, bool frame_lost = false, bool failsafe = false);
Frame encode(const State& state);
std::string hex(const Frame& frame);
} // namespace et08
