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

#include "mainwindow.h"
#include "logger.h"
#include <iostream>

MainWindow::MainWindow() :
    bluetoothSerialPort{BluetoothSerialPort::getBluetoothSerialPort()}
{
    css = Gtk::CssProvider::create();
    css->load_from_resource("/stylesheets/appstyle.css");
    Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(),
                                 css, 
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    ui = Gtk::Builder::create_from_resource("/ui/neonobd_ui.xml");

    viewStack = ui->get_widget<Gtk::Stack>("view_stack");

    if(!viewStack) {
        Logger::error("Could not instantiate view_stack.");
        return;
    }

    set_child(*viewStack);

    home = std::make_unique<Home>(ui, viewStack);
    settings = std::make_unique<Settings>(ui, viewStack);

    set_name("mainwindow");
}
