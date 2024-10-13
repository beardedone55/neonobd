/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2024  Brian LePage
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
#include <gtkmm/cssprovider.h>
#include <gtkmm/window.h>
#include <memory>

using neon::InterfaceType;
using neon::ResponseType;

class MainWindow : public Gtk::Window {
  public:
    MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    virtual ~MainWindow();
    std::shared_ptr<BluetoothSerialPort> bluetoothSerialPort;
    std::shared_ptr<SerialPort> serialPort;
    std::shared_ptr<HardwareInterface> hardwareInterface;
    std::unique_ptr<Home> home;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<Terminal> terminal;
    Gtk::Stack* viewStack;
    Glib::RefPtr<Gtk::Builder> ui;

    void setHardwareInterface(InterfaceType ifType);
    void showPopup(const std::string& message, ResponseType type,
                   const sigc::slot<void(int)>& response);
    void hidePopup();

  protected:
    Gtk::MessageDialog* popup = nullptr;
    sigc::connection popup_response_connection;
    bool popup_shown = false;
    std::unique_ptr<Gtk::MessageDialog> yes_no_dialog;
    std::unique_ptr<Gtk::MessageDialog> text_input_dialog;
    std::unique_ptr<Gtk::MessageDialog> number_input_dialog;
    Glib::RefPtr<Gtk::CssProvider> css;
};
