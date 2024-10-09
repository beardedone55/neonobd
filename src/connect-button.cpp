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

#include "connect-button.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"
#include "neonobd_types.hpp"
#include <glib.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <gtk/gtk.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <sigc++/connection.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/functors/slot.h>
#include <string>
#include <utility>

ConnectButton::ConnectButton(BaseObjectType* cobj,
                             const Glib::RefPtr<Gtk::Builder>& /*builder*/)
    : Gtk::Button{cobj},
      window{dynamic_cast<MainWindow*>(get_ancestor(GTK_TYPE_WINDOW))} {
    signal_clicked().connect(sigc::mem_fun(*this, &ConnectButton::clicked));
}

void ConnectButton::clicked() {
    Logger::debug("Connect button clicked.");
    auto hwif = window->hardwareInterface;

    window->home->disable_all();

    user_prompt_connection = hwif->attach_user_prompt(
        sigc::mem_fun(*this, &ConnectButton::user_prompt));

    connect_complete_connection = hwif->attach_connect_complete(
        sigc::mem_fun(*this, &ConnectButton::connect_complete));

    if (!hwif->connect(window->settings->getSelectedDevice())) {
        connect_complete(false);
    }
}

void ConnectButton::connect_complete(bool result) {
    user_prompt_connection.disconnect();
    connect_complete_connection.disconnect();

    if (result) {
        Logger::debug("Connection to device was successful!");
        window->home->set_connected(true);
    } else {
        Logger::debug("Connection to device failed!");
    }

    window->home->enable_all();
}

void ConnectButton::send_cancel() {
    window->hidePopup();
    auto hwif = window->hardwareInterface;
    Logger::debug("Responding with cancel from user.");
    auto response = Glib::Variant<bool>::create(false);
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

void ConnectButton::user_prompt(const Glib::ustring& prompt,
                                ResponseType responseType,
                                Glib::RefPtr<void> handle) {

    if (user_prompt_handle) {
        // Still have open dialog
        // Respond with error.
        send_cancel();
    }

    sigc::slot<void(int)> user_response;

    switch (responseType) {
    case neon::USER_YN:
        user_response =
            sigc::mem_fun(*this, &ConnectButton::user_yes_no_response);
        break;
    case neon::USER_STRING:
        user_response =
            sigc::mem_fun(*this, &ConnectButton::user_text_response);
        break;
    case neon::USER_INT:
        user_response =
            sigc::mem_fun(*this, &ConnectButton::user_number_response);
        break;
    case neon::USER_NONE:
        user_response =
            sigc::mem_fun(*this, &ConnectButton::user_none_response);
        break;
    }

    window->showPopup(prompt, responseType, user_response);

    user_prompt_handle = std::move(handle);
    Logger::debug("Received prompt: " + prompt);
}

void ConnectButton::user_yes_no_response(int responseCode) {
    Logger::debug("Received response from user: " +
                  std::to_string(responseCode));

    auto response =
        Glib::Variant<bool>::create(responseCode == Gtk::ResponseType::YES);
    window->hidePopup();
    auto hwif = window->hardwareInterface;
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

Glib::ustring ConnectButton::get_user_input(int responseCode,
                                            const char* widget) {
    Logger::debug("User responded with responseCode " +
                  std::to_string(responseCode));
    auto user_interface = window->ui;
    auto text_input =
        user_interface->get_widget<Gtk::Entry>(widget)->get_text();
    if (responseCode != Gtk::ResponseType::OK || text_input.empty()) {
        send_cancel();
        return "";
    }

    window->hidePopup();
    return text_input;
}

void ConnectButton::user_text_response(int responseCode) {
    auto text_input = get_user_input(responseCode, "text_user_input");
    if (!text_input.empty()) {
        auto response = Glib::Variant<Glib::ustring>::create(text_input);
        Logger::debug("Received response from user: " + text_input);
        window->hardwareInterface->respond_from_user(response,
                                                     user_prompt_handle);
        user_prompt_handle.reset();
    }
}

void ConnectButton::user_number_response(int responseCode) {
    auto text_input = get_user_input(responseCode, "number_user_input");
    if (!text_input.empty()) {
        auto response = Glib::Variant<guint32>::create(
            static_cast<guint32>(std::stoi(text_input)));
        Logger::debug("Received response from user: " + text_input);
        window->hardwareInterface->respond_from_user(response,
                                                     user_prompt_handle);
        user_prompt_handle.reset();
    }
}

void ConnectButton::user_none_response(int /*none*/) { window->hidePopup(); }
