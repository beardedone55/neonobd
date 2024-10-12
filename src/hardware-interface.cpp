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

#include "hardware-interface.hpp"
#include <cstddef>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <shared_mutex>
#include <sigc++/connection.h>
#include <sigc++/functors/slot.h>
#include <unistd.h>

sigc::connection
HardwareInterface::attach_connect_complete(const sigc::slot<void(bool)>& slot) {
    return m_complete_connection.connect(slot);
}
sigc::connection HardwareInterface::attach_user_prompt(
    const sigc::slot<void(const Glib::ustring&, ResponseType,
                          Glib::RefPtr<void>)>& slot) {
    return m_request_user_input.connect(slot);
}

size_t HardwareInterface::read(char* buf, std::size_t buf_size) {
    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= 0) {
        auto result = ::read(m_sock_fd, buf, buf_size);
        if (result > -1) {
            return static_cast<size_t>(result);
        }
    }
    return 0;
}

size_t HardwareInterface::write(const char* buf, std::size_t buf_size) {
    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= 0) {
        auto result = ::write(m_sock_fd, buf, buf_size);
        if (result > -1) {
            return static_cast<size_t>(result);
        }
    }
    return 0;
}
