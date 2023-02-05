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

#pragma once
#include "neonobd_types.h"
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <shared_mutex>
#include <sigc++/connection.h>
#include <sigc++/signal.h>
#include <string>
#include <vector>

using neon::ResponseType;

class HardwareInterface {
  public:
    HardwareInterface() = default;
    HardwareInterface(const HardwareInterface &) = delete;
    HardwareInterface &operator=(const HardwareInterface &) = delete;
    virtual ~HardwareInterface() = default;
    virtual bool connect(const Glib::ustring &device_name) = 0;
    sigc::connection
    attach_connect_complete(const sigc::slot<void(bool)> &slot);
    sigc::connection attach_user_prompt(
        const sigc::slot<void(const Glib::ustring &, ResponseType,
                              Glib::RefPtr<void>)> &slot);
    virtual void respond_from_user(const Glib::VariantBase &response,
                                   const Glib::RefPtr<void> &signal_handle) = 0;

    template <typename Container,
              typename Contents = typename Container::value_type>
    std::size_t read(Container &container, std::size_t buf_size = 1024) {
        static_assert(sizeof(Contents) == sizeof(char),
                      "Container used with HardwareInterface::read() must have "
                      "elements with size of char.");
        container.resize(buf_size);
        auto bytecount = read(container.data(), buf_size);
        container.resize(bytecount);
        return bytecount;
    }

    template <typename Container,
              typename Contents = typename Container::value_type>
    std::size_t write(const Container &buf) {
        static_assert(sizeof(Contents) == sizeof(char),
                      "Container used with HardwareInterface::write() must "
                      "have elements with size of char.");
        return write(buf.data(), buf.size());
    }

    virtual void set_timeout(unsigned int) {}

  protected:
    sigc::signal<void(bool)> complete_connection;
    sigc::signal<void(const Glib::ustring &, ResponseType, Glib::RefPtr<void>)>
        request_user_input;
    int sock_fd = -1;
    std::shared_mutex sock_fd_mutex;

    virtual std::size_t read(char *buf, std::size_t size);
    virtual std::size_t write(const char *buf, std::size_t size);
};
