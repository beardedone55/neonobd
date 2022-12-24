/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022  Brian LePage
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

#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>
#include <memory>

#include "home.h"
#include "settings.h"
#include "bluetooth-serial-port.h"
#include "hardware-interface.h"
#include "neonobd_types.h"

using neon::ResponseType;
using neon::InterfaceType;

class MainWindow : public Gtk::Window
{
    public:
        MainWindow();
        MainWindow(const MainWindow&) = delete;
        MainWindow& operator=(const MainWindow&) = delete;
        virtual ~MainWindow();
        std::shared_ptr<BluetoothSerialPort> bluetoothSerialPort;
        std::shared_ptr<HardwareInterface> hardwareInterface;
        std::unique_ptr<Home> home;
        std::unique_ptr<Settings> settings;
        Gtk::Stack *viewStack;
        Glib::RefPtr<Gtk::Builder> ui;

        void setHardwareInterface(InterfaceType ifType);
        void showPopup(const std::string& message,
                       ResponseType type,
                       const sigc::slot<void(int)>& response);
        void hidePopup();

    protected:
        Gtk::MessageDialog* popup;
        sigc::connection popup_response_connection;
        bool popup_shown = false;
        std::unique_ptr<Gtk::MessageDialog> yes_no_dialog;
        std::unique_ptr<Gtk::MessageDialog> text_input_dialog;
        std::unique_ptr<Gtk::MessageDialog> number_input_dialog;
        Glib::RefPtr<Gtk::CssProvider> css;
};

