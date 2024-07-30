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

#include "connection.h"
#include "obd-device.h"
#include <memory>
#include <sigc++/signal.h>

class Obd {
  public:
    sigc::signal<void(bool)> init(std::shared_ptr<ObdDevice> obd_device,
                                  std::shared_ptr<HardwareInterface> hwif);
    sigc::signal<void()> disconnect();

  private:
    std::shared_ptr<ObdDevice> m_obdDevice;
    std::shared_ptr<HardwareInterface> m_hwif;
    bool m_connected = false;
    bool m_connecting = false;
    sigc::signal<void(bool)> m_init_signal;
    Connection m_init_connection;
    bool m_is_CAN = false;
    sigc::signal<void()> m_disconnect_signal;
    Connection m_disconnect_connection;
    bool disconnecting = false;

    void initComplete(bool success);
    void disconnectComplete();
};
