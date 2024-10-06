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

#include "mainwindow.hpp"
#include "bluetooth-serial-port.hpp"
#include "home.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include "settings.hpp"
#include "terminal.hpp"
#include <gdkmm/display.h>
#include <gtk/gtkstyleprovider.h>
#include <gtkmm/builder.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/dialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stack.h>
#include <gtkmm/stylecontext.h>
#include <memory>
#include <sigc++/functors/slot.h>

MainWindow::MainWindow()
    : bluetoothSerialPort{BluetoothSerialPort::get_BluetoothSerialPort()},
      serialPort{new SerialPort}, css{Gtk::CssProvider::create()} {
    css->load_from_resource("/stylesheets/appstyle.css");
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(), css,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    ui = Gtk::Builder::create_from_resource("/ui/neonobd_ui.xml");

    viewStack = ui->get_widget<Gtk::Stack>("view_stack");

    if (viewStack == nullptr) {
        Logger::error("Could not instantiate view_stack.");
        return;
    }

    set_child(*viewStack);

    home = std::make_unique<Home>(this);
    settings = std::make_unique<Settings>(this);
    terminal = std::make_unique<Terminal>(this);
    yes_no_dialog.reset(ui->get_widget<Gtk::MessageDialog>("yes_no_dialog"));
    text_input_dialog.reset(
        ui->get_widget<Gtk::MessageDialog>("text_input_dialog"));
    number_input_dialog.reset(
        ui->get_widget<Gtk::MessageDialog>("number_input_dialog"));

    set_name("mainwindow");
}

MainWindow::~MainWindow() { Logger::debug("Destroying MainWindow."); }

void MainWindow::setHardwareInterface(InterfaceType ifType) {
    switch (ifType) {
    case neon::BLUETOOTH_IF:
        hardwareInterface = bluetoothSerialPort;
        break;
    case neon::SERIAL_IF:
        hardwareInterface = serialPort;
        break;
    }
}

void MainWindow::showPopup(const std::string& message, ResponseType type,
                           const sigc::slot<void(int)>& response) {

    Logger::debug("Showing popup dialog.");

    if (popup_shown) {
        hidePopup();
    }

    switch (type) {
    case neon::USER_YN:
        popup = yes_no_dialog.get();
        break;
    case neon::USER_STRING:
        popup = text_input_dialog.get();
        break;
    case neon::USER_INT:
        popup = number_input_dialog.get();
        break;
    default:
        return;
    }

    popup_shown = true;
    popup_response_connection = popup->signal_response().connect(response);
    popup->set_transient_for(*this);
    popup->set_message(message);
    if (type == neon::USER_YN) {
        popup->set_default_response(Gtk::ResponseType::YES);
    } else {
        popup->set_default_response(Gtk::ResponseType::OK);
    }
    popup->show();
}

void MainWindow::hidePopup() {
    if (!popup_shown) {
        return;
    }

    popup->hide();
    popup_response_connection.disconnect();
    popup_shown = false;
}
