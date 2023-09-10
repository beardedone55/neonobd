/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2023  Brian LePage
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
#include <memory>

class Elm327 : public ObdDevice {
  public:
    Elm327() = default;
    Elm327(const Elm327 &) = delete;
    Elm327 &operator=(const Elm327 &) = delete;
    virtual ~Elm327() = default;

    virtual sigc::connection init(HardwareInterface *hwif,
                                  const sigc::slot<void(bool)> &callback) override;

    virtual Glib::ustring getErrorString() override;

    virtual sigc::connection sendCommand(unsigned char obd_module,
                                         unsigned char obd_service,
                                         const std::vector<unsigned char> &obd_data,
                                         const sigc::slot<void(std::vector<unsigned char>)> &callback) override;
    virtual bool isCAN() override;
    
    virtual sigc::connection disconnect(const sigc::slot<void()> &callback) override;

  private:
    struct Command {
        unsigned char obd_module;
        unsigned char obd_service;
        std::vector<unsigned char> obd_data;
        unsigned int tag;
    };
    HardwareInterface *m_hwif = nullptr;
    bool init_in_progress = false;
    bool init_complete = false;
    std::unique_ptr<std::thread> init_thread;
    sigc::signal<void(bool)> init_signal;
    Glib::Dispatcher dispatcher;
    sigc::connection dispatcher_connection;
    std::queue<Command> cmd_queue;
    
    void init_thread();
    void init_done();
    void command_complete();
}
