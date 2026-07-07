/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2026  Brian LePage
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
#include <future>
#include <memory>
#include <string>
#include <termios.h>

using namespace std::chrono_literals;

class SerialPort : public HardwareInterface {
  public:
    SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    ~SerialPort() override;
    bool connect(const std::string& device_name, std::function<void(bool)> callback) override;
    void respond_from_user(const ResponseVariant&, void*) override {}
    void set_timeout(std::chrono::milliseconds timeout) override;
    
    //Event Loop Processing Methods
    //-------------------------------------------------------
    void process_events(std::chrono::microseconds timeout = 0s) override;
    int get_event_fd() override;

    void set_baudrate(const std::string& baudrate);
    std::vector<std::string> get_valid_baudrates();
    static std::vector<std::string> get_serial_devices();

  private:
    speed_t m_baudrate = B38400;
    static const std::unordered_map<std::string, speed_t> m_baudrates;
    std::future<bool> m_is_connected;
    unsigned char m_timeout = 0;
    static void close_file(std::FILE* file);
    std::unique_ptr<FILE, decltype(&close_file)> m_sock_file;
    std::function<void(bool)> m_connect_callback;
    int m_event_fd[2]  = {-1, -1};

    bool initiate_connection(const std::string& device_name);
    void connect_complete(); 
    static ssize_t read_timed(int fd, char buf[], size_t sz, std::chrono::microseconds timeout);
    void signal_event(const std::string& event_name);
};

