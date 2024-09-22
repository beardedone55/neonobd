/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2023  Brian LePage
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

#include "home.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"

Home::Home(MainWindow* main_window) : window{main_window} {
    auto ui = window->ui;
    // Settings button
    settings_btn = ui->get_widget<Gtk::Button>("settings_button");
    settings_btn->signal_clicked().connect(
        sigc::mem_fun(*this, &Home::settings_clicked));
    enabled_buttons.insert(settings_btn);

    // Connect button
    connect_btn =
        Gtk::Builder::get_widget_derived<ConnectButton>(ui, "connect_button");
    enabled_buttons.insert(connect_btn);

    // Terminal button
    terminal_btn = ui->get_widget<Gtk::Button>("terminal_button");
    // Disable this button until connected.
    terminal_btn->set_sensitive(false);
    terminal_btn->signal_clicked().connect(
        sigc::mem_fun(*this, &Home::terminal_clicked));
}

void Home::settings_clicked() {
    window->viewStack->set_visible_child("settings_view");
}

void Home::terminal_clicked() {
    window->viewStack->set_visible_child("terminal_view");
}

void Home::disable_all() {
    for (auto button : enabled_buttons) {
        button->set_sensitive(false);
    }
}

void Home::enable_all() {
    for (auto button : enabled_buttons) {
        button->set_sensitive(true);
    }
}

void Home::set_connected(bool isConnected) {
    connected = isConnected;

    if (connected) {
        disable_button(connect_btn);
        enable_button(terminal_btn);
    } else {
        enable_button(connect_btn);
        disable_button(terminal_btn);
    }
}

void Home::disable_button(Gtk::Button* button) {
    if (enabled_buttons.contains(button)) {
        button->set_sensitive(false);
        enabled_buttons.erase(button);
    }
}

void Home::enable_button(Gtk::Button* button) {
    button->set_sensitive(true);
    enabled_buttons.insert(button);
}
