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

#include "bluetooth-serial-port.hpp"
#include "hardware-interface.hpp"
#include "home.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include "settings.hpp"
#include "terminal.hpp"
#include "ui_neonobd.h"
#include <QWidget>

using neon::InterfaceType;

class MainWindow : public QWidget {
  public:
    MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    virtual ~MainWindow();
    void setHardwareInterface(InterfaceType ifType);
    int user_get_int(const QString& prompt, bool& ok);
    QString user_get_text(const QString& prompt, bool& ok);
    bool user_get_yes_no(const QString& prompt);

    Home m_home;
    Settings m_settings;
    Terminal m_terminal;

  private:
    Ui::ViewStack m_ui;
    BluetoothSerialPort m_bluetooth_serial_port;
    SerialPort m_serial_port;
    HardwareInterface* m_hardware_interface = nullptr;
    QStackedWidget m_view_stack;

};
