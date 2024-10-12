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

#pragma once

#include "hardware-interface.hpp"
#include <glibmm/ustring.h>
#include <optional>
#include <sigc++/signal.h>
#include <vector>

class ObdDevice {
  public:
    ObdDevice() = default;
    ObdDevice(const ObdDevice&) = delete;
    ObdDevice& operator=(const ObdDevice&) = delete;
    virtual ~ObdDevice() = default;

    virtual sigc::signal<void(bool)>
    init(std::shared_ptr<HardwareInterface> hwif) = 0;

    virtual Glib::ustring get_error_string() const = 0;

    virtual sigc::signal<void(
        const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>
    signal_command_complete() = 0;

    virtual void send_command(unsigned char obd_module,
                              unsigned char obd_service,
                              const std::vector<unsigned char>& obd_data) = 0;

    virtual bool is_connecting() const = 0;

    virtual bool is_connected() const = 0;

    virtual bool is_CAN() const = 0;

    virtual sigc::signal<void()> disconnect() = 0;
};
