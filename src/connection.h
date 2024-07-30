/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2024  Brian LePage
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

#include <sigc++/connection.h>

class Connection {
  public:
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(const sigc::connection& conn) : m_connection{conn} {}
    Connection& operator=(const sigc::connection& conn) {
        disconnect();
        m_connection = conn;
        return *this;
    }
    ~Connection() { disconnect(); }
    void disconnect() {
        if (m_connection)
            m_connection.disconnect();
    }

  private:
    sigc::connection m_connection;
};
