#include "et08/protocol.hpp"

#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// The first eight channel extraction expressions from wefly_et08a.c,
// with explicit parentheses around each complete raw value.
std::array<int, 8> reference_decode(const et08::Frame& b) {
    std::array<int, 8> result{
        (b[1] | ((b[2] & 7) << 8)) - 1024,
        (((b[2] & 0xf8) >> 3) | ((b[3] & 0x3f) << 5)) - 1024,
        (((b[3] & 0xc0) >> 6) | (b[4] << 2) | ((b[5] & 1) << 10)) - 1024,
        (((b[5] & 0xfe) >> 1) | ((b[6] & 0xf) << 7)) - 1024,
        ((b[6] & 0xf0) >> 4) | ((b[7] & 0x7f) << 4),
        ((b[7] & 0x80) >> 7) | (b[8] << 1) | ((b[9] & 3) << 9),
        ((b[9] & 0xfc) >> 2) | ((b[10] & 0x1f) << 6),
        ((b[10] & 0xe0) >> 5) | (b[11] << 3)
    };
    for (int i = 4; i < 8; ++i) {
        const int raw = result[i];
        result[i] = raw == 1024 ? 3 : raw == 1694 ? 2 : raw == 353 ? 1 : 0;
    }
    return result;
}

// Independent streaming decoder checks the complete 176-bit payload, including
// channel boundaries crossing bytes and the unused channels at the end.
et08::Channels unpack(const et08::Frame& frame) {
    et08::Channels result{};
    std::uint32_t buffer = 0;
    unsigned bits = 0;
    std::size_t index = 1;
    for (auto& channel : result) {
        while (bits < 11) {
            buffer |= static_cast<std::uint32_t>(frame[index++]) << bits;
            bits += 8;
        }
        channel = static_cast<std::uint16_t>(buffer & 0x7ff);
        buffer >>= 11;
        bits -= 11;
    }
    return result;
}
} // namespace

int main() {
    try {
        const auto neutral = et08::encode({});
        require(et08::hex(neutral) ==
            "0F 00 04 20 00 01 18 96 B0 84 25 2C 00 04 20 00 01 08 40 00 02 10 80 00 00",
            "Neutral golden frame mismatch");
        require(reference_decode(neutral) == std::array<int, 8>{0, 0, 0, 0, 1, 1, 1, 1},
                "Default state is not online/centered");

        // Every physical axis value, with asymmetric neighboring axes to catch
        // swapped channels, bit spills and sign/offset mistakes.
        for (int axis = 0; axis < 4; ++axis) {
            for (int value = et08::axis_min; value <= et08::axis_max; ++value) {
                et08::State state;
                state.right_x = -517; state.right_y = 321;
                state.left_y = -98; state.left_x = 662;
                int* axes[] = {&state.right_x, &state.right_y, &state.left_y, &state.left_x};
                *axes[axis] = value;
                const auto decoded = reference_decode(et08::encode(state));
                for (int i = 0; i < 4; ++i) require(decoded[i] == *axes[i], "Axis mapping mismatch");
            }
        }
        const et08::Switch two[] = {et08::Switch::Up, et08::Switch::Down};
        const et08::Switch three[] = {et08::Switch::Up, et08::Switch::Middle, et08::Switch::Down};
        for (auto sa : two) for (auto sb : three) for (auto sc : three) for (auto sd : two) {
            et08::State state;
            state.sa = sa; state.sb = sb; state.sc = sc; state.sd = sd;
            const auto decoded = reference_decode(et08::encode(state));
            require(decoded[4] == static_cast<int>(sa) && decoded[5] == static_cast<int>(sb) &&
                    decoded[6] == static_cast<int>(sc) && decoded[7] == static_cast<int>(sd),
                    "Switch decoding mismatch");
        }
        et08::State offline;
        offline.receiver_offline = true;
        offline.left_x = 670;
        require(reference_decode(et08::encode(offline)) == std::array<int, 8>{0, 0, 0, 0, 3, 3, 3, 3},
                "Receiver offline signature mismatch");
        require(et08::hex(et08::encode(offline)) ==
            "0F 00 04 20 00 01 08 40 00 02 10 80 00 04 20 00 01 08 40 00 02 10 80 00 00",
            "Offline golden frame mismatch");
        for (int flags = 0; flags < 4; ++flags) {
            et08::State state;
            state.frame_lost = (flags & 1) != 0;
            state.failsafe = (flags & 2) != 0;
            const auto frame = et08::encode(state);
            require(frame[0] == 0x0f && frame[24] == 0 && frame[23] == flags * 4,
                    "Header/footer/status flags mismatch");
        }
        et08::State clamped;
        clamped.left_x = -10000; clamped.right_y = 10000;
        const auto decoded = reference_decode(et08::encode(clamped));
        require(decoded[3] == -671 && decoded[1] == 670, "Axis clamp mismatch");

        std::mt19937 random(0xE708A);
        for (int i = 0; i < 2000; ++i) {
            et08::Channels raw{};
            for (auto& value : raw) value = static_cast<std::uint16_t>(random() & 0x7ff);
            require(unpack(et08::pack(raw)) == raw, "11-bit round trip mismatch");
        }
        et08::Channels invalid{};
        invalid[15] = 2048;
        bool rejected = false;
        try { (void)et08::pack(invalid); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "Out-of-range raw channel accepted");
        et08::State invalid_switch;
        invalid_switch.sa = et08::Switch::Middle;
        rejected = false;
        try { (void)et08::encode(invalid_switch); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "SA middle accepted in normal mode");
        std::cout << "PASS: golden frames, 5368 axis cases, 36 switch combinations, offline, flags, 2000 payloads\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
