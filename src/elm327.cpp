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

#include "elm327.h"

sigc::connection Elm327::initialize(HardwareInterface *hwif,
                                    const sigc::slot<void(bool)> &callback) {

    auto connection = init_signal.connect(callback); 

    if (m_init_in_progress) {
        init_signal.emit(false);
        return connection;
    }

    m_hwif = hwif;

    dispatcher_connection = dispatcher.connect(sigc::mem_fun(*this, &Elm327::init_done));

    init_thread = std::make_unique<std::thread>([this]() {this->init_thread();});  

    init_in_progress = true;
    return connection;
}

void Elm327::init_thread() {
    
    dispatcher.emit();
}

void Elm327::init_done() {
    init_thread->join();
    init_in_progress = false;
    dispatcher.disconnect(dispatcher_connection);
    init_signal.emit(init_complete);
    dispatcher_connection = dispatcher.connect(sigc::mem_fun(*this, &Elm327::command_complete));
}

void Elm327::command_complete() {
    
}
