/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2024  Brian LePage
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

#include "serial-port.hpp"
#include "logger.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glibmm/ustring.h>
#include <memory>
#include <mutex>
#include <ratio>
#include <shared_mutex>
#include <sigc++/functors/mem_fun.h>
#include <sstream>
#include <string>
#include <system_error>
#include <termios.h>
#include <unordered_map>
#include <vector>

SerialPort::SerialPort() : m_sock_file(nullptr, &SerialPort::close_file) {
    m_dispatcher.connect(sigc::mem_fun(*this, &SerialPort::connect_complete));
}

SerialPort::~SerialPort() {
    if (m_connect_thread && m_connect_thread->joinable()) {
        m_connect_thread->join();
    }

    Logger::debug("Destroying Serial Port");
}

std::vector<Glib::ustring> SerialPort::get_valid_baudrates() {
    std::vector<Glib::ustring> output;
    output.reserve(m_baudrates.size());
    for (const auto& [baudrate_str, baudrate] : m_baudrates) {
        output.emplace_back(baudrate_str);
    }
    return output;
}

std::vector<Glib::ustring> SerialPort::get_serial_devices() {
    std::ifstream procfile("/proc/tty/drivers");
    if (!procfile) {
        return {};
    }

    std::unordered_map<std::filesystem::path, std::vector<std::string>>
        file_prefixes;
    while (!procfile.eof()) {
        constexpr int BUFFER_SIZE = 128;
        std::array<char, BUFFER_SIZE> line{};
        procfile.getline(line.data(), BUFFER_SIZE);
        std::stringstream line_ss(line.data());
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

        const std::filesystem::path path(words[1]);

        file_prefixes[path.parent_path()].push_back(path.filename().string());
    }

    std::vector<Glib::ustring> device_list;
    for (auto& [folder, prefixes] : file_prefixes) {
        for (const auto& dir_entry :
             std::filesystem::directory_iterator(folder)) {
            for (auto& prefix : prefixes) {
                if (dir_entry.path().filename().string().starts_with(prefix)) {
                    device_list.emplace_back(dir_entry.path().string());
                    break;
                }
            }
        }
    }

    return device_list;
}

void SerialPort::set_baudrate(const Glib::ustring& new_baudrate) {
    m_baudrate = m_baudrates.at(new_baudrate);
}

void SerialPort::connect_complete() {
    m_connect_thread->join();
    m_connect_thread.reset();
    m_complete_connection.emit(m_connected);
}

void SerialPort::initiate_connection(const Glib::ustring& device_name) {
    try {
        const std::lock_guard lock(m_sock_fd_mutex);

        m_sock_file.reset(std::fopen(device_name.c_str(), "r+e"));

        if (!m_sock_file) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to open serial port.");
        }

        m_sock_fd = fileno(m_sock_file.get());

        // Device opened.  Set BAUD rate and other port settings.

        termios port_settings = {};

        if (tcgetattr(m_sock_fd, &port_settings) == -1 ||
            cfsetispeed(&port_settings, m_baudrate) == -1 ||
            cfsetospeed(&port_settings, m_baudrate) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to set baudrate");
        }

        cfmakeraw(&port_settings);
        port_settings.c_cc[VMIN] = 0;
        port_settings.c_cc[VTIME] = m_timeout;

        if (tcsetattr(m_sock_fd, TCSANOW, &port_settings) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to apply serial port settings");
        }

        m_connected = true;

    } catch (const std::system_error& e) {
        Logger::error(e.what());
        if (m_sock_file) {
            m_sock_file.reset();
            m_sock_fd = -1;
        }
    }

    m_dispatcher.emit();
}

bool SerialPort::connect(const Glib::ustring& device_name) {
    if (m_sock_file) {
        Logger::error("Connection to serial port already exists.");
        return false;
    }

    if (m_connect_thread) {
        Logger::error("Connection attempt already in progress.");
        return false;
    }

    m_connect_thread = std::make_unique<std::thread>(
        [this, device_name]() { this->initiate_connection(device_name); });

    return true;
}

void SerialPort::set_timeout(std::chrono::milliseconds timeout) {
    const std::chrono::duration<int64_t, std::deci> deciseconds =
        std::chrono::duration_cast<std::chrono::duration<int64_t, std::deci>>(
            timeout);

    m_timeout = (deciseconds.count() > UCHAR_MAX)
                    ? UCHAR_MAX
                    : static_cast<unsigned char>(deciseconds.count());

    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd == -1) {
        return;
    }

    termios port_settings = {};
    if (tcgetattr(m_sock_fd, &port_settings) == -1) {
        Logger::error("Failed to retrieve serial port settings.");
        return;
    }

    port_settings.c_cc[VMIN] = 0;
    port_settings.c_cc[VTIME] = m_timeout;

    if (tcsetattr(m_sock_fd, TCSANOW, &port_settings) == -1) {
        Logger::error("Failed to set serial port timeout.");
    }
}

void SerialPort::close_file(std::FILE* file) {
    static_cast<void>(std::fclose(file));
}
