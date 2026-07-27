/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2023-2026  Brian LePage
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

#include "event-handler.hpp"
#include "hardware-interface.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

class ObdDevice : public EventHandler {
  public:
    ObdDevice() { init_event_handler(); }
    ObdDevice(const ObdDevice&) = delete;
    ObdDevice& operator=(const ObdDevice&) = delete;
    virtual ~ObdDevice() = default;

    virtual void init(HardwareInterface* hwif,
                      std::function<void(bool)> callback) = 0;

    virtual std::string get_error_string() const = 0;

    using CommandCallback = std::function<void(
        const std::unordered_map<unsigned int, std::vector<unsigned char>>&)>;

    virtual void send_command(unsigned char obd_module,
                              unsigned char obd_service,
                              const std::vector<unsigned char>& obd_data,
                              CommandCallback callback) = 0;

    virtual bool is_connecting() const = 0;
    virtual bool is_connected() const = 0;
    virtual bool is_CAN() const = 0;
    virtual disconnect(std::function<void()> callback) = 0;
};
