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

#include "obd-device.h"
#include <thread>
#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>
#include "connection.h"
#include <memory>
#include <mutex>
#include <semaphore>
#include <queue>
#include <unordered_map>

class Elm327 : public ObdDevice, public sigc::trackable {
  public:
    Elm327() = default;
    Elm327(const Elm327 &) = delete;
    Elm327 &operator=(const Elm327 &) = delete;
    virtual ~Elm327();

    virtual sigc::signal<void(bool)> 
        init(std::shared_ptr<HardwareInterface> hwif) override;
    
    virtual Glib::ustring getErrorString() const override;

    virtual sigc::signal<void(const std::unordered_map<unsigned int, 
                              std::vector<unsigned char>>&)>
                                signal_command_complete() override;

    virtual void sendCommand(unsigned char obd_address,
                             unsigned char obd_service,
                             const std::vector<unsigned char> &obd_data) override;

    virtual bool isCAN() const override;

    virtual bool isConnecting() const override;

    virtual bool isConnected() const override;
    
    virtual sigc::signal<void()> disconnect() override;

  private:
    struct Command {
        unsigned int obd_address;
        unsigned char obd_service;
        std::vector<unsigned char> obd_data;
    };

    struct Completion {
        std::unordered_map<unsigned int,std::vector<unsigned char>> obd_data;
    };

    std::shared_ptr<HardwareInterface> hwif;
    volatile bool disconnect_in_progress = false;
    bool init_in_progress = false;
    bool init_complete = false;
    std::unique_ptr<std::thread> init_thread;
    sigc::signal<void(bool)> init_signal;
    sigc::signal<void()> disconnect_signal;
    Glib::Dispatcher init_complete_dispatcher;
    Connection init_complete_connection;
    Glib::Dispatcher command_complete_dispatcher;
    Connection command_complete_connection;
    Glib::Dispatcher command_thread_exit_dispatcher;
    Connection command_thread_exit_connection;
    std::mutex cmd_queue_lock;
    std::binary_semaphore cmd_semaphore;
    std::queue<Command> cmd_queue;
    std::unique_ptr<std::thread> command_thread;
    std::mutex completion_queue_lock;
    std::queue<Completion> completion_queue;
    sigc::signal<void(const std::unordered_map<unsigned int,std::vector<unsigned char>>&)> 
    command_complete_signal;
    unsigned int current_obd_address = 0;
    Glib::ustring error_string;

    void initThread();
    void init_done();
    void sendCompletion(Completion&& completion);
    Command getNextCmd();
    Completion getNextCompletion();
    static std::string command_to_string(const Command& command);
    Completion string_to_completion(const std::string& s);
    void commandThread();
    void command_complete();
    void command_thread_exit();
    std::string send_command(const std::string& cmd);
    int protocol;
    
    //ELM327 Config commands
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
