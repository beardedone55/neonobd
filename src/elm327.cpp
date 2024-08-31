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

#include "elm327.h"
#include "neonobd_exceptions.h"
#include <iomanip>
#include <sstream>

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

sigc::signal<void(bool)>
Elm327::init(std::shared_ptr<HardwareInterface> hwif) {
    if (m_init_thread || m_init_complete) {
        throw neon::InvalidState("Invalid state to issue init request.");
    }

    m_hwif = hwif;

    m_init_complete_connection =
        m_init_complete_dispatcher.connect([this]() { init_done(); });

    m_init_thread =
        std::make_unique<std::thread>([this]() { init_thread(); });

    return m_init_signal;
}

Glib::ustring Elm327::get_error_string() const { return m_error_string; }

void Elm327::init_thread() {
    if (!reset()) {
        m_error_string = "ELM327 reset failed.";
        goto exit;
    }

    if (!disable_echo()) {
        m_error_string = "ELM327 failed to disable command echo.";
        goto exit;
    }

    if (!enable_headers()) {
        m_error_string = "ELM237 failed to enable headers.";
        goto exit;
    }
    /*
        if(!disable_spaces()) {
            m_error_string = "ELM237 failed to disable spaces.";
            goto exit;
        }
    */
    if (!scan_protocol()) {
        m_error_string = "ELM327 failed to determine OBD protocol.";
        goto exit;
    }

    m_init_complete = true;

    m_command_thread =
        std::make_unique<std::thread>([this]() { command_thread(); });

exit:
    m_init_complete_dispatcher.emit();
}

void Elm327::init_done() {
    m_init_thread->join();
    m_init_thread.reset();
    if (m_init_complete) {
        m_command_complete_connection = m_command_complete_dispatcher.connect(
            [this]() { command_complete(); });
        m_command_thread_exit_connection = m_command_thread_exit_dispatcher.connect(
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

    if (!m_init_complete || m_disconnect_in_progress)
        return;
    std::lock_guard lock(m_cmd_queue_lock);
    m_cmd_queue.emplace(Command({obd_address, obd_service, obd_data}));
    m_cmd_semaphore.release();
}

bool Elm327::is_CAN() const {
    // Protocol numbers above 5 are CAN bus protocols.
    return m_protocol > 5;
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
    std::lock_guard lock(m_completion_queue_lock);
    m_completion_queue.emplace(completion);
}

template <typename T> static T get_next(std::queue<T>& q, std::mutex& m) {
    std::lock_guard lock(m);
    auto result = std::move(q.front());
    q.pop();
    return result;
}

Elm327::Command Elm327::get_next_cmd() {
    return get_next(m_cmd_queue, m_cmd_queue_lock);
}

Elm327::Completion Elm327::get_next_completion() {
    return get_next(m_completion_queue, m_completion_queue_lock);
}

std::string Elm327::command_to_string(const Elm327::Command& command) {
    std::stringstream cmd;
    cmd << std::setw(2) << std::hex << command.obd_service;
    for (auto d : command.obd_data) {
        cmd << d;
    }
    cmd << std::setw(0) << "\r";
    return cmd.str();
}

static unsigned int get_header(std::stringstream& ss, bool is_CAN) {
    unsigned int result = 0;
    int count = is_CAN ? 1 : 3;
    for (int i = 0; i < count && ss; ++i) {
        if (ss.peek() == '\r')
            return 0;
        unsigned int temp;
        ss >> std::hex >> temp;
        result <<= 8;
        result += temp;
    }
    return result;
}

Elm327::Completion Elm327::string_to_completion(const std::string& s) {
    Completion cpl;
    bool is_can = is_CAN();

    std::stringstream ss(s);
    while (ss && ss.peek() != '>') {
        unsigned int header = get_header(ss, is_can);
        if (header == 0)
            break; // Some kind of error occurred...

        while (ss && ss.peek() != '\r') {
            unsigned int temp;
            ss >> std::hex >> temp;
            cpl.obd_data[header].push_back(static_cast<unsigned char>(temp));
        }

        if (ss && ss.peek() == '\r')
            ss.get();
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

            m_command_complete_dispatcher.emit(); // Let main thread know there is
                                                // a response on the queue.
        }
    }
    m_command_thread_exit_dispatcher.emit();
}

void Elm327::command_complete() {
    auto cpl = get_next_completion();
    m_command_complete_signal.emit(cpl.obd_data);
}

template <typename T> static void clear_queue(std::queue<T>& q) {
    while (!q.empty())
        q.pop();
}

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
    std::string response, buffer;
    m_hwif->write(cmd);
    size_t prompt_position;
    do {
        m_hwif->read(buffer);
        prompt_position = buffer.find('>');
        response.append(buffer);
    } while (buffer.size() > 0 && prompt_position == std::string::npos);

    // What if we don't get a prompt back????  Let caller take care of it?

    return response;
}

// Elm327 Config commands

static bool check_response(const std::string& response) {
    return response.find("OK") != std::string::npos &&
           response.find('>') != std::string::npos;
}

bool Elm327::set_header(unsigned int header) {
    std::stringstream cmd;
    cmd << "ATSH" << std::hex << std::setw(6) << std::setfill('0') << header
        << "\r";

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
    if (!check_response(send_command("ATSP0\r")))
        return false;

    if (send_command("0100\r").find('>') == std::string::npos)
        return false;

    auto response = send_command("ATDPN\r");

    // Expected response is Ax, where x is the protocol number
    if (response.size() < 2)
        return false;

    if (response.find('>') == std::string::npos)
        return false;

    m_protocol = response[1];
    if (m_protocol >= '1' && m_protocol <= '9') {
        m_protocol = m_protocol - '0';
    } else if (m_protocol >= 'A' && m_protocol <= 'F') {
        m_protocol = m_protocol - 'A' + 10;
    } else {
        m_protocol = 0;
        return false;
    }

    return true;
}
