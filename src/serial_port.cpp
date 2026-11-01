#include "et08/serial_port.hpp"

#include <libserialport.h>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace et08 {
namespace {
#ifdef _WIN32
// libserialport uses the Windows ANSI APIs, while Slint requires UTF-8.
std::string convert_encoding(const std::string& input, UINT source, UINT destination) {
    if (input.empty()) return {};
    const int wide_size = MultiByteToWideChar(source, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (!wide_size) throw std::runtime_error("Could not decode serial port text");
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (!MultiByteToWideChar(source, 0, input.data(), static_cast<int>(input.size()), wide.data(), wide_size))
        throw std::runtime_error("Could not decode serial port text");
    const int size = WideCharToMultiByte(destination, 0, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Could not encode serial port text");
    std::string output(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(destination, 0, wide.data(), wide_size, output.data(), size, nullptr, nullptr))
        throw std::runtime_error("Could not encode serial port text");
    return output;
}
std::string from_native(const std::string& value) { return convert_encoding(value, CP_ACP, CP_UTF8); }
std::string to_native(const std::string& value) { return convert_encoding(value, CP_UTF8, CP_ACP); }
#else
std::string from_native(const std::string& value) { return value; }
std::string to_native(const std::string& value) { return value; }
#endif

void check(sp_return result, const char* operation) {
    if (result >= SP_OK) return;
    std::string detail;
    if (result == SP_ERR_FAIL) {
        const auto message = std::unique_ptr<char, decltype(&sp_free_error_message)>(
            sp_last_error_message(), sp_free_error_message);
        if (message) {
            detail = from_native(message.get());
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
        const char* name = sp_get_port_name(*entry);
        ports.push_back({from_native(name ? name : ""), from_native(description ? description : "")});
    }
    return ports;
}

void SerialPort::open(const std::string& port) {
    close();
    try {
        const auto native_port = to_native(port);
        check(sp_get_port_by_name(native_port.c_str(), &port_), "Find serial port");
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
