/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2023  Brian LePage
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

#include "hardware-interface.h"
#include <sigc++/connection.h>
#include <sigc++/functors/slot.h>
#include <glibmm/ustring.h>
#include <vector>

class ObdDevice {
  public:
    ObdDevice() = default;
    ObdDevice(const ObdDevice &) = delete;
    ObdDevice &operator=(const ObdDevice &) = delete;
    virtual ~ObdDevice() = default;

    virtual sigc::connection init(HardwareInterface *hwif,
                                  const sigc::slot<void(bool)> &callback) = 0;

    virtual Glib::ustring getErrorString() = 0;

    virtual sigc::connection sendCommand(unsigned char obd_module,
                                         unsigned char obd_service,
                                         const std::vector<unsigned char> &obd_data,
                                         const sigc::slot<void(std::vector<unsigned char>)> &callback) = 0;
    virtual bool isCAN() = 0;
    
    virtual sigc::connection disconnect(const sigc::slot<void()> &callback) = 0;
}