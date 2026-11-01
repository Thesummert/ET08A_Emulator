#include "et08/sender.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace et08 {
Sender::~Sender() { stop(); }

void Sender::start(std::string port, int hz, const State& initial) {
    if (port.empty()) throw std::invalid_argument("Please select or enter a serial port");
    if (hz < 10 || hz > 100) throw std::invalid_argument("Send rate must be 10..100 Hz");
    (void)encode(initial); // Validate before opening any hardware.
    stop();
    {
        std::lock_guard lock(mutex_);
        state_ = initial;
        stopping_ = false;
        status_ = {true, false, 0, {}};
    }
    try {
        thread_ = std::thread([this, port = std::move(port), hz] { run(port, hz); });
    } catch (...) {
        std::lock_guard lock(mutex_);
        status_.running = false;
        throw;
    }
}

void Sender::stop() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void Sender::update(const State& state) {
    std::lock_guard lock(mutex_);
    state_ = state;
}

SenderStatus Sender::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

void Sender::run(const std::string& port, int hz) {
    using Clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(1'000'000'000 / hz);
    try {
        transport_.open(port);
        {
            std::lock_guard lock(mutex_);
            status_.connected = true;
        }
        auto deadline = Clock::now();
        for (;;) {
            State next;
            {
                std::lock_guard lock(mutex_);
                if (stopping_) break;
                next = state_;
            }
            transport_.write(encode(next));
            std::unique_lock lock(mutex_);
            ++status_.frames;
            deadline += period;
            // Missed deadlines are skipped; never send a burst of stale frames.
            if (deadline < Clock::now()) deadline = Clock::now() + period;
            if (wake_.wait_until(lock, deadline, [this] { return stopping_; })) break;
        }
    } catch (const std::exception& error) {
        std::lock_guard lock(mutex_);
        status_.error = error.what();
    } catch (...) {
        std::lock_guard lock(mutex_);
        status_.error = "Unknown serial transport error";
    }
    transport_.close();
    std::lock_guard lock(mutex_);
    status_.running = false;
    status_.connected = false;
}
} // namespace et08
