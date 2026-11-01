#include "app.h"
#include "et08/protocol.hpp"
#include "et08/serial_port.hpp"
#include "et08/sender.hpp"

#include <slint.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
et08::State read_state(const slint::ComponentHandle<AppWindow>& ui) {
    const auto three_way = [](int index) {
        return index == 0 ? et08::Switch::Up : index == 1 ? et08::Switch::Middle : et08::Switch::Down;
    };
    et08::State state;
    state.right_x = ui->get_right_x();
    state.right_y = ui->get_right_y();
    state.left_y = ui->get_left_y();
    state.left_x = ui->get_left_x();
    state.sa = ui->get_sa() == 0 ? et08::Switch::Up : et08::Switch::Down;
    state.sb = three_way(ui->get_sb());
    state.sc = three_way(ui->get_sc());
    state.sd = ui->get_sd() == 0 ? et08::Switch::Up : et08::Switch::Down;
    state.receiver_offline = ui->get_receiver_offline();
    state.frame_lost = ui->get_frame_lost();
    state.failsafe = ui->get_failsafe();
    return state;
}
} // namespace

int main() {
    try {
        auto ui = AppWindow::create();
        et08::SerialPort serial;
        et08::Sender sender(serial);
        std::vector<et08::PortInfo> ports;
        bool was_running = false;
        std::string shown_error;
        auto refresh = [&] {
            try {
                ports = et08::list_ports();
                std::vector<slint::SharedString> labels;
                for (const auto& port : ports) {
                    const auto label = port.description.empty() ? port.name : port.name + " — " + port.description;
                    labels.emplace_back(label.c_str());
                }
                ui->set_ports(std::make_shared<slint::VectorModel<slint::SharedString>>(labels));
                if (ui->get_port_name().empty() && !ports.empty())
                    ui->set_port_name(ports.front().name.c_str());
                ui->set_status_text(ports.empty() ? "未发现串口，可手动填写设备名" : "串口列表已刷新，点击开始发送以连接");
            } catch (const std::exception& error) {
                ui->set_status_text(error.what());
            }
        };
        ui->on_refresh_ports(refresh);
        ui->on_select_port([&](int index) {
            if (index >= 0 && static_cast<std::size_t>(index) < ports.size())
                ui->set_port_name(ports[static_cast<std::size_t>(index)].name.c_str());
        });
        ui->on_toggle_send([&] {
            try {
                if (sender.status().running) {
                    sender.stop();
                    was_running = false;
                    ui->set_status_text("发送已停止，串口已关闭");
                } else {
                    shown_error.clear();
                    was_running = false;
                    const auto name = ui->get_port_name();
                    sender.start(std::string(name.data(), name.size()), ui->get_hz(), read_state(ui));
                    ui->set_status_text("正在打开串口…");
                }
                ui->set_running(sender.status().running);
            } catch (const std::exception& error) {
                ui->set_status_text(error.what());
            }
        });
        refresh();

        // Slint is accessed only on its UI thread. The worker receives plain C++ data.
        slint::Timer timer;
        timer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(20), [&] {
            const auto state = read_state(ui);
            sender.update(state);
            const auto frame = et08::encode(state);
            ui->set_frame_hex(et08::hex(frame).c_str());
            const auto raw = et08::channels(state);
            std::ostringstream values;
            for (int i = 0; i < 8; ++i) {
                if (i) values << "   ";
                values << "CH" << i + 1 << "=" << raw[static_cast<std::size_t>(i)];
            }
            ui->set_channel_text(values.str().c_str());
            const auto status = sender.status();
            ui->set_running(status.running);
            ui->set_connected(status.connected);
            ui->set_frame_count(std::to_string(status.frames).c_str());
            if (!status.error.empty() && status.error != shown_error) {
                ui->set_status_text(("发送已停止：" + status.error).c_str());
                shown_error = status.error;
            } else if (status.connected && !was_running) {
                ui->set_status_text(("发送中 · 100000 8E2 · " + std::to_string(ui->get_hz()) + " Hz").c_str());
                shown_error.clear();
            }
            was_running = status.connected;
        });
        ui->run();
        timer.stop();
        sender.stop();
    } catch (const std::exception& error) {
        std::cerr << "ET08A emulator: " << error.what() << '\n';
        return 1;
    }
}
