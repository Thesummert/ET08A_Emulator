#pragma once

#include "et08/protocol.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace et08 {
// Only the worker thread touches a Transport. Writes must have a bounded timeout.
class Transport {
public:
    virtual ~Transport() = default;
    virtual void open(const std::string& port) = 0;
    virtual void write(const Frame& frame) = 0;
    virtual void close() noexcept = 0;
};

struct SenderStatus {
    bool running = false;
    bool connected = false;
    std::uint64_t frames = 0; // Frames fully accepted by the serial driver.
    std::string error;
};

class Sender {
public:
    explicit Sender(Transport& transport) : transport_(transport) {}
    ~Sender();
    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;
    // start/stop are called by the UI thread; update/status are thread-safe.
    void start(std::string port, int hz, const State& initial);
    void stop();
    void update(const State& state);
    SenderStatus status() const;

private:
    void run(const std::string& port, int hz);
    Transport& transport_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    State state_;
    SenderStatus status_;
    bool stopping_ = false;
};
} // namespace et08
