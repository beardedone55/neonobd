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

#pragma once
#include "neonobd_types.hpp"
#include <shared_mutex>
#include <QObject>
#include <QString>
#include <QVariant>

using neon::ResponseType;

class HardwareInterface : public QObject {

  Q_OBJECT

  public:
    HardwareInterface() = default;
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    virtual ~HardwareInterface() = default;
    virtual bool connect(const QString& device_name) = 0;
    virtual void respond_from_user(const QVariant& response,
                                   std::shared_ptr<void> signal_handle) = 0;

    template <typename Container,
              typename Contents = typename Container::value_type>
    size_t read(Container& container, std::size_t buf_size = 1024) {
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
    size_t write(const Container& buf) {
        static_assert(sizeof(Contents) == sizeof(char),
                      "Container used with HardwareInterface::write() must "
                      "have elements with size of char.");
        return write(buf.data(), buf.size());
    }

    virtual void set_timeout(std::chrono::milliseconds) {}

  signals:
	void complete_connection(bool);
	void request_user_input(const QString&, ResponseType, std::shared_ptr<void>); 

  protected:
    int m_sock_fd = -1;
    std::shared_mutex m_sock_fd_mutex;

    virtual size_t read(char* buf, std::size_t size);
    virtual size_t write(const char* buf, std::size_t size);
};
