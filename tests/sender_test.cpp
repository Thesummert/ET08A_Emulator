#include "et08/sender.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class FakeTransport final : public et08::Transport {
public:
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<et08::Frame> frames;
    std::vector<std::chrono::steady_clock::time_point> times;
    bool fail_open = false;
    bool fail_write = false;
    int delay_ms = 0;
    int closed = 0;

    void open(const std::string&) override {
        if (fail_open) throw std::runtime_error("open failed");
    }
    void write(const et08::Frame& frame) override {
        if (fail_write) throw std::runtime_error("write failed");
        if (delay_ms) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        std::lock_guard lock(mutex);
        frames.push_back(frame);
        times.push_back(std::chrono::steady_clock::now());
        wake.notify_all();
    }
    void close() noexcept override { ++closed; }
    void wait_count(std::size_t count) {
        std::unique_lock lock(mutex);
        require(wake.wait_for(lock, 1500ms, [&] { return frames.size() >= count; }), "Frame wait timed out");
    }
    std::size_t count() {
        std::lock_guard lock(mutex);
        return frames.size();
    }
};

void wait_stopped(et08::Sender& sender) {
    const auto limit = std::chrono::steady_clock::now() + 1500ms;
    while (sender.status().running && std::chrono::steady_clock::now() < limit)
        std::this_thread::sleep_for(2ms);
    require(!sender.status().running, "Worker did not stop after failure");
    sender.stop(); // Join before checking transport lifetime.
}
} // namespace

int main() {
    try {
        FakeTransport transport;
        et08::Sender sender(transport);
        sender.start("fake", 70, {});
        transport.wait_count(4);
        et08::State changed;
        changed.left_x = -671;
        changed.sb = et08::Switch::Middle;
        sender.update(changed);
        // One in-flight old snapshot is permitted when the UI updates the state.
        const auto at_update = transport.count();
        transport.wait_count(at_update + 3);
        sender.stop();
        require(transport.frames.back() == et08::encode(changed), "Worker did not pick up new state");
        require(sender.status().frames == transport.frames.size(), "Frame counter mismatch");
        require(!sender.status().running && !sender.status().connected, "Stop left worker connected");
        require(transport.closed == 1, "Transport not closed on stop");
        const auto count = transport.count();
        std::this_thread::sleep_for(30ms);
        require(transport.count() == count, "Frames emitted after stop");
        for (std::size_t i = 1; i < transport.times.size(); ++i)
            require(transport.times[i] - transport.times[i - 1] >= 10ms, "70 Hz frames sent in a burst");

        transport.fail_open = true;
        sender.start("fake", 70, {});
        wait_stopped(sender);
        require(sender.status().error == "open failed", "Open error lost");
        require(transport.closed == 2, "Open failure did not release transport");
        transport.fail_open = false;
        transport.fail_write = true;
        sender.start("fake", 70, {});
        wait_stopped(sender);
        require(sender.status().error == "write failed", "Write error lost");
        require(sender.status().frames == 0, "Failed frame counted as sent");

        transport.fail_write = false;
        transport.delay_ms = 30; // Longer than a 100 Hz period: must not catch up in a burst.
        const auto first_delayed = transport.count();
        sender.start("fake", 100, {});
        transport.wait_count(first_delayed + 3);
        sender.stop();
        require(sender.status().error.empty(), "Restart retained stale error");
        for (std::size_t i = first_delayed + 1; i < transport.times.size(); ++i)
            require(transport.times[i] - transport.times[i - 1] >= 38ms, "Missed deadlines were not skipped");

        for (int rate : {0, 9, 101}) {
            bool rejected = false;
            try { sender.start("fake", rate, {}); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid frequency accepted");
        }
        bool rejected = false;
        try { sender.start("", 70, {}); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Empty port accepted");
        std::cout << "PASS: periodic send, live updates, stop, open/write failure, restart, missed deadlines\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
