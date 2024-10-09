/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2023  Brian LePage
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
#include "hardware-interface.hpp"
#include <chrono>
#include <cstdio>
#include <glibmm/dispatcher.h>
#include <memory>
#include <termios.h>
#include <thread>

class SerialPort : public HardwareInterface, public sigc::trackable {
  public:
    SerialPort();
    ~SerialPort() override;
    bool connect(const Glib::ustring& device_name) override;
    void respond_from_user(const Glib::VariantBase&,
                           const Glib::RefPtr<void>&) override {}
    void set_timeout(std::chrono::milliseconds timeout) override;

    void set_baudrate(const Glib::ustring& baudrate);
    std::vector<Glib::ustring> get_valid_baudrates();
    static std::vector<Glib::ustring> get_serial_devices();

  private:
    speed_t m_baudrate = B38400;
    const std::unordered_map<std::string, speed_t> m_baudrates = {
        {"9600", B9600},   {"19200", B19200},   {"38400", B38400},
        {"57600", B57600}, {"115200", B115200}, {"230400", B230400}};
    Glib::Dispatcher m_dispatcher;
    std::unique_ptr<std::thread> m_connect_thread;
    bool m_connected = false;
    unsigned char m_timeout = 0;
    static void close_file(std::FILE* file);
    std::unique_ptr<FILE,decltype(&close_file)> m_sock_file; 

    void initiate_connection(const Glib::ustring& device_name);
    void connect_complete();
};
