#pragma once

#include "et08/sender.hpp"

#include <string>
#include <vector>

struct sp_port;

namespace et08 {
struct PortInfo {
    std::string name;
    std::string description;
};
std::vector<PortInfo> list_ports();

class SerialPort final : public Transport {
public:
    SerialPort() = default;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    ~SerialPort() override { close(); }
    void open(const std::string& port) override;
    void write(const Frame& frame) override;
    void close() noexcept override;
private:
    sp_port* port_ = nullptr;
    bool opened_ = false;
};
} // namespace et08
