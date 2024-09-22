/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2023-2024  Brian LePage
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "connection.hpp"
#include "obd-device.hpp"

#ifndef CPPCHECK
#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>
#endif

#include <memory>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <unordered_map>

class Elm327 : public ObdDevice, public sigc::trackable {
  public:
    Elm327() = default;
    Elm327(const Elm327&) = delete;
    Elm327& operator=(const Elm327&) = delete;
    ~Elm327() override;

    sigc::signal<void(bool)>
    init(std::shared_ptr<HardwareInterface> hwif) override;

    Glib::ustring get_error_string() const override;

    sigc::signal<void(
        const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>
    signal_command_complete() override;

    void send_command(unsigned char obd_address, unsigned char obd_service,
                      const std::vector<unsigned char>& obd_data) override;

    bool is_CAN() const override;

    bool is_connecting() const override;

    bool is_connected() const override;

    sigc::signal<void()> disconnect() override;

  private:
    struct Command {
        unsigned int obd_address;
        unsigned char obd_service;
        std::vector<unsigned char> obd_data;
    };

    struct Completion {
        std::unordered_map<unsigned int, std::vector<unsigned char>> obd_data;
    };

    std::shared_ptr<HardwareInterface> m_hwif;
    volatile bool m_disconnect_in_progress = false;
    bool m_init_in_progress = false;
    bool m_init_complete = false;
    std::unique_ptr<std::thread> m_init_thread;
    sigc::signal<void(bool)> m_init_signal;
    sigc::signal<void()> m_disconnect_signal;
    Glib::Dispatcher m_init_complete_dispatcher;
    Connection m_init_complete_connection;
    Glib::Dispatcher m_command_complete_dispatcher;
    Connection m_command_complete_connection;
    Glib::Dispatcher m_command_thread_exit_dispatcher;
    Connection m_command_thread_exit_connection;
    std::mutex m_cmd_queue_lock;
    std::binary_semaphore m_cmd_semaphore;
    std::queue<Command> m_cmd_queue;
    std::unique_ptr<std::thread> m_command_thread;
    std::mutex m_completion_queue_lock;
    std::queue<Completion> m_completion_queue;
    sigc::signal<void(
        const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>
        m_command_complete_signal;
    unsigned int m_current_obd_address = 0;
    Glib::ustring m_error_string;
    int m_protocol = 0;

    void init_thread();
    void init_done();
    void send_completion(Completion&& completion);
    Command get_next_cmd();
    Completion get_next_completion();
    static std::string command_to_string(const Command& command);
    Completion string_to_completion(const std::string& s);
    void command_thread();
    void command_complete();
    void command_thread_exit();
    std::string send_command(const std::string& cmd);

    // ELM327 Config commands
    bool set_header(unsigned int header);
    bool reset();
    bool enable_echo();
    bool disable_echo();
    bool enable_headers();
    bool disable_headers();
    bool enable_spaces();
    bool disable_spaces();
    bool scan_protocol();
};
