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
    if (init_thread) {
        init_thread->join();
    }

    if (command_thread) {
        disconnect_in_progress = true;
        cmd_semaphore.release();
        command_thread->join();
    }
}

sigc::signal<void(bool)>
Elm327::init(std::shared_ptr<HardwareInterface> interface) {
    if (init_thread || init_complete) {
        throw neon::InvalidState("Invalid state to issue init request.");
    }

    hwif = interface;

    init_complete_connection =
        init_complete_dispatcher.connect([this]() { init_done(); });

    init_thread =
        std::make_unique<std::thread>([this]() { this->initThread(); });

    return init_signal;
}

Glib::ustring Elm327::getErrorString() const { return error_string; }

void Elm327::initThread() {
    if (!reset()) {
        error_string = "ELM327 reset failed.";
        goto exit;
    }

    if (!disable_echo()) {
        error_string = "ELM327 failed to disable command echo.";
        goto exit;
    }

    if (!enable_headers()) {
        error_string = "ELM237 failed to enable headers.";
        goto exit;
    }
    /*
        if(!disable_spaces()) {
            error_string = "ELM237 failed to disable spaces.";
            goto exit;
        }
    */
    if (!scan_protocol()) {
        error_string = "ELM327 failed to determine OBD protocol.";
        goto exit;
    }

    init_complete = true;

    command_thread =
        std::make_unique<std::thread>([this]() { this->commandThread(); });

exit:
    init_complete_dispatcher.emit();
}

void Elm327::init_done() {
    init_thread->join();
    init_thread.reset();
    if (init_complete) {
        command_complete_connection = command_complete_dispatcher.connect(
            [this]() { command_complete(); });
        command_thread_exit_connection = command_thread_exit_dispatcher.connect(
            [this]() { command_thread_exit(); });
    }
    init_signal.emit(init_complete);
}

sigc::signal<
    void(const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>
Elm327::signal_command_complete() {
    return command_complete_signal;
}

void Elm327::sendCommand(unsigned char obd_address, unsigned char obd_service,
                         const std::vector<unsigned char>& obd_data) {

    if (!init_complete || disconnect_in_progress)
        return;
    std::lock_guard lock(cmd_queue_lock);
    cmd_queue.emplace(Command({obd_address, obd_service, obd_data}));
    cmd_semaphore.release();
}

bool Elm327::isCAN() const {
    // Protocol numbers above 5 are CAN bus protocols.
    return protocol > 5;
}

bool Elm327::isConnecting() const { return !!init_thread; }

bool Elm327::isConnected() const { return init_complete; }

sigc::signal<void()> Elm327::disconnect() {
    if (disconnect_in_progress || !init_complete) {
        throw neon::InvalidState("Invalid state to issue disconnect request.");
    }

    disconnect_in_progress = true;
    cmd_semaphore.release();
    init_complete = false;

    return disconnect_signal;
}

void Elm327::sendCompletion(Completion&& completion) {
    std::lock_guard lock(completion_queue_lock);
    completion_queue.emplace(completion);
}

template <typename T> static T getNext(std::queue<T>& q, std::mutex& m) {
    std::lock_guard lock(m);
    auto result = std::move(q.front());
    q.pop();
    return result;
}

Elm327::Command Elm327::getNextCmd() {
    return getNext(cmd_queue, cmd_queue_lock);
}

Elm327::Completion Elm327::getNextCompletion() {
    return getNext(completion_queue, completion_queue_lock);
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

static unsigned int getHeader(std::stringstream& ss, bool is_CAN) {
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
    bool is_CAN = isCAN();

    std::stringstream ss(s);
    while (ss && ss.peek() != '>') {
        unsigned int header = getHeader(ss, is_CAN);
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

void Elm327::commandThread() {
    while (!disconnect_in_progress) {
        cmd_semaphore.acquire();
        while (!cmd_queue.empty() && !disconnect_in_progress) {
            auto command = getNextCmd();
            if (current_obd_address != command.obd_address) {
                set_header(command.obd_address);
                current_obd_address = command.obd_address;
            }
            auto response = send_command(command_to_string(command));

            sendCompletion(string_to_completion(response));

            command_complete_dispatcher.emit(); // Let main thread know there is
                                                // a response on the queue.
        }
    }
    command_thread_exit_dispatcher.emit();
}

void Elm327::command_complete() {
    auto cpl = getNextCompletion();
    command_complete_signal.emit(cpl.obd_data);
}

template <typename T> static void clear_queue(std::queue<T>& q) {
    while (!q.empty())
        q.pop();
}

void Elm327::command_thread_exit() {
    command_thread->join();
    command_thread.reset();
    init_complete = false;
    disconnect_in_progress = false;
    clear_queue(cmd_queue);
    clear_queue(completion_queue);
    disconnect_signal.emit();
}

std::string Elm327::send_command(const std::string& cmd) {
    std::string response, buffer;
    hwif->write(cmd);
    size_t prompt_position;
    do {
        hwif->read(buffer);
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

    protocol = response[1];
    if (protocol >= '1' && protocol <= '9') {
        protocol = protocol - '0';
    } else if (protocol >= 'A' && protocol <= 'F') {
        protocol = protocol - 'A' + 10;
    } else {
        protocol = 0;
        return false;
    }

    return true;
}
