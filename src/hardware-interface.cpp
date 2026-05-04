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

#include "hardware-interface.hpp"
#include "logger.hpp"
#include <cstddef>
#include <shared_mutex>
#include <unistd.h>

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
