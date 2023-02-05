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

#include "serial-port.h"
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

const std::unordered_map<std::string, speed_t> SerialPort::baudrates = {
    {"9600", B9600},   {"19200", B19200},   {"38400", B38400},
    {"57600", B57600}, {"115200", B115200}, {"230400", B230400}};

SerialPort::SerialPort() {
    dispatcher.connect(sigc::mem_fun(*this, &SerialPort::connect_complete));
}

SerialPort::~SerialPort() {
    if (connect_thread && connect_thread->joinable()) {
        connect_thread->join();
    }
    Logger::debug("Destroying Serial Port");
}

std::vector<Glib::ustring> SerialPort::get_valid_baudrates() {
    std::vector<Glib::ustring> output;
    for (auto &[baudrate_str, baudrate] : baudrates) {
        output.push_back(baudrate_str);
    }
    return output;
}

std::vector<Glib::ustring> SerialPort::get_serial_devices() {
    std::ifstream procfile("/proc/tty/drivers");
    if (!procfile)
        return {};

    std::unordered_map<std::filesystem::path, std::vector<std::string>>
        file_prefixes;
    while (!procfile.eof()) {
        char line[128];
        procfile.getline(line, sizeof(line));
        std::stringstream line_ss(line);
        std::vector<std::string> words;
        while (!line_ss.eof()) {
            std::string word;
            line_ss >> word;
            if (line_ss) {
                words.push_back(word);
            }
        }

        if (words.size() <= 2 || words[words.size() - 1] != "serial") {
            continue;
        }

        std::filesystem::path path(words[1]);

        file_prefixes[path.parent_path()].push_back(path.filename().string());
    }

    std::vector<Glib::ustring> device_list;
    for (auto &[folder, prefixes] : file_prefixes) {
        for (auto &dir_entry : std::filesystem::directory_iterator(folder)) {
            for (auto &prefix : prefixes) {
                if (dir_entry.path().filename().string().starts_with(prefix)) {
                    device_list.push_back(dir_entry.path().string());
                    break;
                }
            }
        }
    }

    return device_list;
}

void SerialPort::set_baudrate(const Glib::ustring &new_baudrate) {
    baudrate = baudrates.at(new_baudrate);
}

void SerialPort::connect_complete() {
    connect_thread->join();
    connect_thread.reset();
    complete_connection.emit(connected);
}

void SerialPort::initiate_connection(const Glib::ustring &device_name) {
    try {
        std::lock_guard lock(sock_fd_mutex);
        sock_fd = open(device_name.c_str(), O_RDWR);

        if (sock_fd == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to open serial port.");
        }

        // Device opened.  Set BAUD rate and other port settings.

        termios port_settings;

        if (tcgetattr(sock_fd, &port_settings) == -1 ||
            cfsetispeed(&port_settings, baudrate) == -1 ||
            cfsetospeed(&port_settings, baudrate) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to set baudrate");
        }

        cfmakeraw(&port_settings);
        port_settings.c_cc[VMIN] = 0;
        port_settings.c_cc[VTIME] = timeout;

        if (tcsetattr(sock_fd, TCSANOW, &port_settings) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to apply serial port settings");
        }

        connected = true;

    } catch (const std::system_error &e) {
        Logger::error(e.what());
        if (sock_fd != -1) {
            close(sock_fd);
            sock_fd = -1;
        }
    }

    dispatcher.emit();
}

bool SerialPort::connect(const Glib::ustring &device_name) {
    if (sock_fd != -1) {
        Logger::error("Connection to serial port already exists.");
        return false;
    }

    if (connect_thread) {
        Logger::error("Connection attempt already in progress.");
        return false;
    }

    connect_thread = std::make_unique<std::thread>(
        [this, device_name]() { this->initiate_connection(device_name); });

    return true;
}

void SerialPort::set_timeout(unsigned int milliseconds) {
    auto deciseconds = milliseconds / 100;

    timeout = (deciseconds > UCHAR_MAX)
                  ? UCHAR_MAX
                  : static_cast<unsigned char>(deciseconds);

    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd == -1)
        return;

    termios port_settings;
    if (tcgetattr(sock_fd, &port_settings) == -1) {
        Logger::error("Failed to retrieve serial port settings.");
        return;
    }

    port_settings.c_cc[VMIN] = 0;
    port_settings.c_cc[VTIME] = timeout;

    if (tcsetattr(sock_fd, TCSANOW, &port_settings) == -1) {
        Logger::error("Failed to set serial port timeout.");
    }
}
