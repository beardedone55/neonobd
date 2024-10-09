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

#include "elm327.hpp"
#include "hardware-interface.hpp"
#include "neonobd_exceptions.hpp"
#include <cstddef>
#include <glibmm/ustring.h>
#include <iomanip>
#include <ios>
#include <memory>
#include <mutex>
#include <sigc++/signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

Elm327::~Elm327() {
    if (m_init_thread) {
        m_init_thread->join();
    }

    if (m_command_thread) {
        m_disconnect_in_progress = true;
        m_cmd_semaphore.release();
        m_command_thread->join();
    }
}

sigc::signal<void(bool)> Elm327::init(std::shared_ptr<HardwareInterface> hwif) {
    if (m_init_thread || m_init_complete) {
        throw neon::InvalidState("Invalid state to issue init request.");
    }

    m_hwif = hwif;

    m_init_complete_connection =
        m_init_complete_dispatcher.connect([this]() { init_done(); });

    m_init_thread = std::make_unique<std::thread>([this]() { init_thread(); });

    return m_init_signal;
}

Glib::ustring Elm327::get_error_string() const { return m_error_string; }

void Elm327::init_thread() {
    try {
        if (!reset()) {
            throw std::runtime_error("ELM327 reset failed.");
        }

        if (!disable_echo()) {
            throw std::runtime_error("ELM327 failed to disable command echo.");
        }

        if (!enable_headers()) {
            throw std::runtime_error("ELM237 failed to enable headers.");
        }
        /*
            if(!disable_spaces()) {
                throw std::runtime_error("ELM237 failed to disable spaces.");
            }
        */
        if (!scan_protocol()) {
            throw std::runtime_error(
                "ELM327 failed to determine OBD protocol.");
        }

        m_init_complete = true;

        m_command_thread =
            std::make_unique<std::thread>([this]() { command_thread(); });
    } catch (std::runtime_error& e) {
        m_error_string = e.what();
    }
    m_init_complete_dispatcher.emit();
}

void Elm327::init_done() {
    m_init_thread->join();
    m_init_thread.reset();
    if (m_init_complete) {
        m_command_complete_connection = m_command_complete_dispatcher.connect(
            [this]() { command_complete(); });
        m_command_thread_exit_connection =
            m_command_thread_exit_dispatcher.connect(
                [this]() { command_thread_exit(); });
    }
    m_init_signal.emit(m_init_complete);
}

sigc::signal<
    void(const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>
Elm327::signal_command_complete() {
    return m_command_complete_signal;
}

void Elm327::send_command(unsigned char obd_address, unsigned char obd_service,
                          const std::vector<unsigned char>& obd_data) {

    if (!m_init_complete || m_disconnect_in_progress) {
        return;
    }
    const std::lock_guard lock(m_cmd_queue_lock);
    m_cmd_queue.emplace(Command({obd_address, obd_service, obd_data}));
    m_cmd_semaphore.release();
}

bool Elm327::is_CAN() const {
    // Protocol numbers 6 and above are CAN bus protocols.
    constexpr int MIN_CAN_PROTOCOL = 6;
    return m_protocol >= MIN_CAN_PROTOCOL;
}

bool Elm327::is_connecting() const { return !!m_init_thread; }

bool Elm327::is_connected() const { return m_init_complete; }

sigc::signal<void()> Elm327::disconnect() {
    if (m_disconnect_in_progress || !m_init_complete) {
        throw neon::InvalidState("Invalid state to issue disconnect request.");
    }

    m_disconnect_in_progress = true;
    m_cmd_semaphore.release();
    m_init_complete = false;

    return m_disconnect_signal;
}

void Elm327::send_completion(Completion&& completion) {
    const std::lock_guard lock(m_completion_queue_lock);
    m_completion_queue.emplace(std::move(completion));
}

namespace {
template <typename T> T get_next(std::queue<T>& queue, std::mutex& mutex) {
    const std::lock_guard lock(mutex);
    auto result = std::move(queue.front());
    queue.pop();
    return result;
}
} // namespace

Elm327::Command Elm327::get_next_cmd() {
    return get_next(m_cmd_queue, m_cmd_queue_lock);
}

Elm327::Completion Elm327::get_next_completion() {
    return get_next(m_completion_queue, m_completion_queue_lock);
}

std::string Elm327::command_to_string(const Elm327::Command& command) {
    std::stringstream cmd;
    cmd << std::setw(2) << std::hex << command.obd_service;
    for (auto data : command.obd_data) {
        cmd << data;
    }
    cmd << std::setw(0) << "\r";
    return cmd.str();
}

namespace {
unsigned int get_header(std::stringstream& bytestream, bool is_CAN) {
    unsigned int result = 0;
    const int count = is_CAN ? 1 : 3;
    for (int i = 0; i < count && bytestream; ++i) {
        if (bytestream.peek() == '\r') {
            return 0;
        }
        unsigned int temp = 0;
        bytestream >> std::hex >> temp;
        static constexpr unsigned int BITS_PER_BYTE = 8;
        result <<= BITS_PER_BYTE;
        result += temp;
    }
    return result;
}
} // namespace

Elm327::Completion
Elm327::string_to_completion(const std::string& bytes) const {
    Completion cpl;
    const bool is_can = is_CAN();

    std::stringstream bytestream(bytes);
    while (bytestream && bytestream.peek() != '>') {
        const unsigned int header = get_header(bytestream, is_can);
        if (header == 0) {
            break; // Some kind of error occurred...
        }

        while (bytestream && bytestream.peek() != '\r') {
            unsigned int temp = 0;
            bytestream >> std::hex >> temp;
            cpl.obd_data[header].push_back(static_cast<unsigned char>(temp));
        }

        if (bytestream && bytestream.peek() == '\r') {
            bytestream.get();
        }
    }

    return cpl;
}

void Elm327::command_thread() {
    while (!m_disconnect_in_progress) {
        m_cmd_semaphore.acquire();
        while (!m_cmd_queue.empty() && !m_disconnect_in_progress) {
            auto command = get_next_cmd();
            if (m_current_obd_address != command.obd_address) {
                set_header(command.obd_address);
                m_current_obd_address = command.obd_address;
            }
            auto response = send_command(command_to_string(command));

            send_completion(string_to_completion(response));

            m_command_complete_dispatcher.emit(); // Let main thread know there
                                                  // is a response on the queue.
        }
    }
    m_command_thread_exit_dispatcher.emit();
}

void Elm327::command_complete() {
    auto cpl = get_next_completion();
    m_command_complete_signal.emit(cpl.obd_data);
}

namespace {
template <typename T> void clear_queue(std::queue<T>& queue) {
    while (!queue.empty()) {
        queue.pop();
    }
}
} // namespace

void Elm327::command_thread_exit() {
    m_command_thread->join();
    m_command_thread.reset();
    m_init_complete = false;
    m_disconnect_in_progress = false;
    clear_queue(m_cmd_queue);
    clear_queue(m_completion_queue);
    m_disconnect_signal.emit();
}

std::string Elm327::send_command(const std::string& cmd) {
    std::string response;
    std::string buffer;
    m_hwif->write(cmd);
    size_t prompt_position = 0;
    do {
        m_hwif->read(buffer);
        prompt_position = buffer.find('>');
        response.append(buffer);
    } while (!buffer.empty() && prompt_position == std::string::npos);

    // What if we don't get a prompt back????  Let caller take care of it?

    return response;
}

// Elm327 Config commands

namespace {
bool check_response(const std::string& response) {
    return response.find("OK") != std::string::npos &&
           response.find('>') != std::string::npos;
}
} // namespace

bool Elm327::set_header(unsigned int header) {
    std::stringstream cmd;
    static constexpr unsigned int HEADER_SIZE_NIBBLES = 6;
    cmd << "ATSH" << std::hex << std::setw(HEADER_SIZE_NIBBLES)
        << std::setfill('0') << header << "\r";

    auto response = send_command(cmd.str());
    return check_response(response);
}

bool Elm327::reset() {
    auto response = send_command("ATZ\r");
    return response.find('>') != std::string::npos;
}

bool Elm327::enable_echo() { return check_response(send_command("ATE1\r")); }

bool Elm327::disable_echo() { return check_response(send_command("ATE0\r")); }

bool Elm327::enable_headers() { return check_response(send_command("ATH1\r")); }

bool Elm327::disable_headers() {
    return check_response(send_command("ATH0\r"));
}

bool Elm327::enable_spaces() { return check_response(send_command("ATS1\r")); }

bool Elm327::disable_spaces() { return check_response(send_command("ATS0\r")); }

bool Elm327::scan_protocol() {
    if (!check_response(send_command("ATSP0\r"))) {
        return false;
    }

    if (send_command("0100\r").find('>') == std::string::npos) {
        return false;
    }

    auto response = send_command("ATDPN\r");

    // Expected response is Ax, where x is the protocol number
    if (response.size() < 2) {
        return false;
    }

    if (response.find('>') == std::string::npos) {
        return false;
    }

    static constexpr int HEX_BASE = 16;
    m_protocol = std::stoi(response.substr(1, 1), nullptr, HEX_BASE);

    return m_protocol > 0;
}
