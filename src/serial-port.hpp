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
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <termios.h>
#include <utility>

using namespace std::chrono_literals;

class SerialPort : public HardwareInterface {
  public:
    SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    ~SerialPort() override;
    bool connect(const std::string& device_name,
                 std::function<void(bool)> callback) override;
    void respond_from_user(const ResponseVariant&, void*) override {}
    void set_timeout(std::chrono::milliseconds timeout) override;

    void set_baudrate(const std::string& baudrate);
    static std::vector<std::string> get_valid_baudrates();
    static std::vector<std::string> get_serial_devices();

  private:
    speed_t m_baudrate = B38400;
    static constexpr std::array<std::pair<std::string, speed_t>, 6>
        m_baudrates = {{{"9600", B9600},
                        {"19200", B19200},
                        {"38400", B38400},
                        {"57600", B57600},
                        {"115200", B115200},
                        {"230400", B230400}}};

    std::future<bool> m_is_connected;
    unsigned char m_timeout = 0;
    static void close_file(std::FILE* file);
    std::unique_ptr<FILE, decltype(&close_file)> m_sock_file;
    std::function<void(bool)> m_connect_callback;

    bool initiate_connection(const std::string& device_name);
    void connect_complete();
    void process_event(std::string_view event) override;
};
