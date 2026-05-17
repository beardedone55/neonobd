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

#include "serial-port.hpp"
#include "logger.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <poll.h>
#include <ratio>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

const std::unordered_map<std::string, speed_t> SerialPort::m_baudrates = {
    {"9600", B9600},   {"19200", B19200},   {"38400", B38400},
    {"57600", B57600}, {"115200", B115200}, {"230400", B230400}};

SerialPort::SerialPort() : m_sock_file(nullptr, close_file) {
    if(pipe2(m_event_fd, O_DIRECT | O_CLOEXEC) < 0) {
        throw std::runtime_error("Creation of event pipe failed...");
    }
}

SerialPort::~SerialPort() {
    for(auto fd : m_event_fd) {
        close(fd);    
    }
    Logger::debug("Destroying Serial Port");
}

ssize_t SerialPort::read_timed(int fd, char buf[], size_t sz, std::chrono::microseconds timeout) {
    pollfd pfd = { .fd = fd, .events = POLLIN };
    int ret = poll(&pfd, 1, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    if(ret > 0) {
        return ::read(fd, buf, sz);
    } else if (ret < 0) {
        throw std::runtime_error("Poll of read fd failed...");
    }
    return 0;
} 

void SerialPort::process_events(std::chrono::microseconds timeout) {
    auto start = std::chrono::system_clock::now();
    auto end = start;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    do {
        char buf[PIPE_BUF];
        ssize_t count = (timeout == 0s) ? ::read(m_event_fd[0], buf, sizeof(buf)) :
            read_timed(m_event_fd[0], buf, sizeof(buf), timeout - duration);
        if(count > 0) {
            buf[PIPE_BUF-1] = '\0';
            static const std::unordered_map<std::string, std::function<void(SerialPort*)>> callbacks =
                {
                    {std::string("ConnectComplete"), &SerialPort::connect_complete }
                };

            std::string function_name = buf;
            if(callbacks.contains(function_name)) {
                callbacks.at(function_name)(this);
            }
        } else if (count < 0) {
            throw std::runtime_error("Read of event pipe fd failed...");
        }
        end = std::chrono::system_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    } while (duration < timeout);
}

int SerialPort::get_event_fd() {
    return m_event_fd[0];
}

std::vector<std::string> SerialPort::get_valid_baudrates() {
    std::vector<std::string> output;
    output.reserve(m_baudrates.size());
    for (const auto& [baudrate_str, baudrate] : m_baudrates) {
        output.emplace_back(baudrate_str);
    }
    return output;
}

std::vector<std::string> SerialPort::get_serial_devices() {
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

    std::vector<std::string> device_list;
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

void SerialPort::set_baudrate(const std::string& new_baudrate) {
    m_baudrate = m_baudrates.at(new_baudrate);
}

void SerialPort::connect_complete() {
    if(m_connect_callback) {
        m_connect_callback(m_is_connected.get());
        m_connect_callback = nullptr;
    }
}

void SerialPort::signal_event(const std::string& event_name) {
    ::write(m_event_fd[1], event_name.c_str(), event_name.size());
}

bool SerialPort::initiate_connection(const std::string& device_name) {
    bool connected = false;
    try {
        const std::lock_guard lock(m_sock_fd_mutex);

        // I really don't know how to fix this warning.  I need to use
        // fopen, so I can get the file descriptor.  The file will be
        // automatically closed when m_sock_file is reset or destroyed.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        m_sock_file.reset(std::fopen(device_name.c_str(), "r+e"));

        if (!m_sock_file) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to open serial port.");
        }

        // Documentation for fileno says that it is provided by
        //<cstdio>
        // NOLINTNEXTLINE(misc-include-cleaner)
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

        connected = true;

    } catch (const std::system_error& e) {
        Logger::error(e.what());
        if (m_sock_file) {
            m_sock_file.reset();
            m_sock_fd = -1;
        }
    }

    signal_event("ConnectComplete");

    return connected;
}

bool SerialPort::connect(const std::string& device_name, std::function<void(bool)> callback) {
    if (m_sock_file) {
        Logger::error("Connection to serial port already exists.");
        return false;
    }

    if (m_is_connected.valid()) {
        Logger::error("Connection attempt already in progress.");
        return false;
    }

    m_connect_callback = callback;
    m_is_connected = std::async(std::launch::async, &SerialPort::initiate_connection, this, device_name);

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
    // This function is called when the unique_ptr
    // that owns the file is destroyed or reset.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    static_cast<void>(std::fclose(file));
}
