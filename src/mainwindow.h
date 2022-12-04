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

#include "home.h"
#include "settings.h"
#include "bluetooth-serial-port.h"

class MainWindow : public Gtk::Window
{
    public:
        MainWindow();
        virtual ~MainWindow();
        void connect_view_button(Gtk::Button &button, 
                                 Gtk::Widget *currentView,
                                 Gtk::Widget *nextView);
        BluetoothSerialPort &bluetoothSerialPort;
        Home home;
        Settings settings;
    protected:
        Glib::RefPtr<Gtk::CssProvider> css;
        void switchView(Gtk::Widget *oldview, Gtk::Widget *newview);
        void showView(Gtk::Widget &view);
};

