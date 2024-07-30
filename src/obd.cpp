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

#include "obd.h"
#include "neonobd_exceptions.h"

sigc::signal<void(bool)>
Obd::init(std::shared_ptr<ObdDevice> obd_device, 
          std::shared_ptr<HardwareInterface> hwif) {

    if(m_connected || m_connecting)
        throw neon::InvalidState("Invalid state to initialize OBD device.");

    m_obdDevice = obd_device;
    m_hwif = hwif;

    m_init_connection = 
        obd_device->init(hwif).connect([this](bool success){initComplete(success);});

    m_connecting = true;

    return m_init_signal;
}

void Obd::initComplete(bool success) {
    m_connected = success;
    m_connecting = false;
    m_is_CAN = m_obdDevice->isCAN();
    m_init_connection.disconnect();
    m_init_signal.emit(success);
}

sigc::signal<void()> Obd::disconnect() {
    if(!m_connected || disconnecting)
        throw neon::InvalidState("Invalid state to disconnect OBD device.");
    m_disconnect_connection = 
        m_obdDevice->disconnect().connect([this](){disconnectComplete();});
    return m_disconnect_signal;
}

void Obd::disconnectComplete() {
    m_disconnect_connection.disconnect();
    m_disconnect_signal.emit();
}
