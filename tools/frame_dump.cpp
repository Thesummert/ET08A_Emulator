#include "et08/protocol.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    et08::State state;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--offline") state.receiver_offline = true;
            else if (arg == "--help") {
                std::cout << "et08a_frame [--rx N] [--ry N] [--ly N] [--lx N] [--offline]\n"
                             "Print one SBUS frame without accessing a serial port. Axes: -671..670.\n";
                return 0;
            } else {
                if (arg != "--rx" && arg != "--ry" && arg != "--ly" && arg != "--lx")
                    throw std::invalid_argument("Unknown option: " + arg);
                if (++i == argc) throw std::invalid_argument("Missing axis value");
                std::size_t used = 0;
                const std::string value_arg = argv[i];
                const int value = std::stoi(value_arg, &used);
                if (used != value_arg.size() || value < et08::axis_min || value > et08::axis_max)
                    throw std::invalid_argument("Axis must be -671..670");
                if (arg == "--rx") state.right_x = value;
                else if (arg == "--ry") state.right_y = value;
                else if (arg == "--ly") state.left_y = value;
                else state.left_x = value;
            }
        }
        std::cout << et08::hex(et08::encode(state)) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
