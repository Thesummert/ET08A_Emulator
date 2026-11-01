#include "et08/serial_port.hpp"

#include <libserialport.h>
#include <memory>
#include <stdexcept>

namespace et08 {
namespace {
void check(sp_return result, const char* operation) {
    if (result >= SP_OK) return;
    std::string detail;
    if (result == SP_ERR_FAIL) {
        char* message = sp_last_error_message();
        if (message) {
            detail = message;
            sp_free_error_message(message);
        }
    } else if (result == SP_ERR_SUPP) detail = "Unsupported by this serial driver";
    else if (result == SP_ERR_ARG) detail = "Invalid serial argument";
    else if (result == SP_ERR_MEM) detail = "Out of memory";
    throw std::runtime_error(std::string(operation) + ": " + detail);
}
} // namespace

std::vector<PortInfo> list_ports() {
    sp_port** list = nullptr;
    check(sp_list_ports(&list), "List serial ports");
    const auto cleanup = std::unique_ptr<sp_port*, decltype(&sp_free_port_list)>(list, sp_free_port_list);
    std::vector<PortInfo> ports;
    for (auto entry = list; *entry; ++entry) {
        const char* description = sp_get_port_description(*entry);
        ports.push_back({sp_get_port_name(*entry), description ? description : ""});
    }
    return ports;
}

void SerialPort::open(const std::string& port) {
    close();
    try {
        check(sp_get_port_by_name(port.c_str(), &port_), "Find serial port");
        check(sp_open(port_, SP_MODE_WRITE), "Open serial port");
        opened_ = true;
        sp_port_config* config = nullptr;
        check(sp_new_config(&config), "Allocate serial configuration");
        const auto cleanup = std::unique_ptr<sp_port_config, decltype(&sp_free_config)>(config, sp_free_config);
        // STM32's UART_WORDLENGTH_9B + EVEN parity corresponds to PC-side 8E2.
        check(sp_set_config_baudrate(config, 100000), "Set 100000 baud");
        check(sp_set_config_bits(config, 8), "Set 8 data bits");
        check(sp_set_config_parity(config, SP_PARITY_EVEN), "Set even parity");
        check(sp_set_config_stopbits(config, 2), "Set 2 stop bits");
        check(sp_set_config_flowcontrol(config, SP_FLOWCONTROL_NONE), "Disable flow control");
        check(sp_set_config(port_, config), "Apply 100000 8E2 configuration");
        check(sp_flush(port_, SP_BUF_OUTPUT), "Flush serial output");
    } catch (...) {
        close();
        throw;
    }
}

void SerialPort::write(const Frame& frame) {
    if (!opened_) throw std::runtime_error("Serial port is closed");
    // No unbounded driver queue: allow at most one frame before writing another.
    const auto pending = sp_output_waiting(port_);
    check(pending, "Check serial output queue");
    if (pending != 0) throw std::runtime_error("Serial output stalled; transmission stopped");
    const auto written = sp_blocking_write(port_, frame.data(), frame.size(), 100);
    check(written, "Write SBUS frame");
    if (static_cast<std::size_t>(written) != frame.size())
        throw std::runtime_error("Serial write timed out; incomplete SBUS frame");
}

void SerialPort::close() noexcept {
    if (port_) {
        if (opened_) {
            (void)sp_flush(port_, SP_BUF_OUTPUT);
            (void)sp_close(port_);
        }
        sp_free_port(port_);
    }
    port_ = nullptr;
    opened_ = false;
}
} // namespace et08
