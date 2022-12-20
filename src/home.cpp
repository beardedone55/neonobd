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
#include "settings.h"
#include "logger.h"

Home::Home(const Glib::RefPtr<Gtk::Builder>& ui, MainWindow* window) : 
    window{window}
{
    //Settings button
    settings_btn = ui->get_widget<Gtk::Button>("settings_button");
    settings_btn->signal_clicked().connect(
        sigc::mem_fun(*this, &Home::settings_clicked));

    //Connect button
    connect_btn = ui->get_widget<Gtk::Button>("connect_button");
    connect_btn->signal_clicked().connect(
        sigc::mem_fun(*this, &Home::connect_clicked));
}

void Home::settings_clicked() {
    window->viewStack->set_visible_child("settings_view");
}

void Home::connect_clicked() {
    Logger::debug("Connect button clicked.");
    window->bluetoothSerialPort->connect(window->settings->getSelectedDevice(),
                                         sigc::mem_fun(*this,&Home::connect_complete),
                                         sigc::mem_fun(*this,&Home::user_prompt));
}

void Home::connect_complete(bool result) {
    if(result) {
        Logger::debug("Connection to device was successful!");
    } else {
        Logger::debug("Connection to device failed!");
    }
}

void Home::user_prompt(Glib::ustring prompt,
                       HardwareInterface::ResponseType responseType,
                       Glib::RefPtr<void>) {
    Logger::debug("Received prompt: " + prompt);
}

