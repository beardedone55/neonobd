/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022  Brian LePage
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

#include "hardware-interface.h"
#include <unistd.h>

sigc::connection
HardwareInterface::attach_connect_complete(const sigc::slot<void(bool)>& slot) {
    return complete_connection.connect(slot);
}
sigc::connection
HardwareInterface::attach_user_prompt(const sigc::slot<void(const Glib::ustring&,
                                      ResponseType,
                                      Glib::RefPtr<void>)>& slot) {
    return request_user_input.connect(slot);
}

std::vector<char>::size_type
HardwareInterface::read(std::vector<char>& buf,
                        std::vector<char>::size_type buf_size)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        buf.resize(buf_size);
        auto result = ::read(sock_fd, buf.data(), buf_size * sizeof(char));
        if(result != -1)
            return result;
    }
    return 0;
}

std::vector<char>::size_type HardwareInterface::write(const std::vector<char>& buf)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        auto result = ::write(sock_fd, buf.data(), buf.size() * sizeof(char));
        if(result != -1)
            return result;
    }
    return 0;
}

std::size_t
HardwareInterface::read(std::string& buf, std::size_t buf_size) {
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        buf.resize(buf_size);
        auto result = ::read(sock_fd, buf.data(), buf.size());
        if(result != -1) {
            buf.resize(result);
            return result;
        }
    }
    return 0;
}

std::size_t HardwareInterface::write(const std::string& buf)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        auto result = ::write(sock_fd, buf.data(), buf.size());
        if(result != -1)
            return result;
    }
    return 0;
}

