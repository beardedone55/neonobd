/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2026  Brian LePage
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

#include "event-handler.hpp"
#include "logger.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <linux/limits.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <unistd.h>

EventHandler::~EventHandler() {
    for (auto event_fd : m_event_fd) {
        if (event_fd >= 0) {
            close(event_fd);
        }
    }
}

void EventHandler::init_event_handler() {
    Logger::debug << "Initializing event handler.\n";
    if (pipe2(m_event_fd.data(), O_DIRECT | O_CLOEXEC) < 0) {
        throw std::runtime_error("Creation of event pipe failed...");
    }
}

void EventHandler::process_events() {
    std::array<char, PIPE_BUF> buf{};
    pollfd pfd = {.fd = m_event_fd.at(0), .events = POLLIN, .revents = 0};

    size_t count = 0;
    do {
        const std::span<char> subarray =
            std::span(buf).last(buf.size() - count);

        const auto res =
            ::read(m_event_fd.at(0), subarray.data(), subarray.size() - 1);

        if (res > 0) {
            count += static_cast<size_t>(res);
            buf.at(count) = '\0';
            for (std::span<char> substr(buf); count > 0;) {
                const std::string_view event = substr.data();

                // We didn't get a whole string
                if (event.size() == count) {
                    // Copy the string to the beginning of the buffer,
                    // and break out to read the rest of the string.
                    std::memmove(buf.data(), event.data(), count);
                    break;
                }
                process_event(event);
                substr = substr.last(substr.size() - event.size() - 1);
                count -= event.size() + 1;
            }
        } else if (res < 0) {
            throw std::runtime_error("Read of event pipe fd failed...");
        }
    } while (poll(&pfd, 1, 0) > 0 && (static_cast<uint32_t>(pfd.revents) &
                                      static_cast<uint32_t>(POLLIN)) != 0);
}

int EventHandler::get_event_fd() const { return m_event_fd.at(0); }

void EventHandler::signal_event(const std::string& event) {
    ::write(m_event_fd.at(1), event.c_str(), event.size() + 1);
}
