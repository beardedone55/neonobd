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

Home::Home(MainWindow* window) : 
    window{window}
{
    auto ui = window->ui;
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
    auto hwif = window->hardwareInterface;

    user_prompt_connection =
        hwif->attach_user_prompt(sigc::mem_fun(*this,&Home::user_prompt));

    connect_complete_connection =
        hwif->attach_connect_complete(sigc::mem_fun(*this,&Home::connect_complete));

    if(!hwif->connect(window->settings->getSelectedDevice()))
        connect_complete(false);
}

void Home::connect_complete(bool result) {
    user_prompt_connection.disconnect();
    connect_complete_connection.disconnect();

    if(result) {
        Logger::debug("Connection to device was successful!");
    } else {
        Logger::debug("Connection to device failed!");
    }
}

void Home::send_cancel() {
    window->hidePopup();
    auto hwif = window->hardwareInterface;
    Logger::debug("Responding with cancel from user.");
    auto response = Glib::Variant<bool>::create(false);
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

void Home::user_prompt(Glib::ustring prompt,
                       ResponseType responseType,
                       Glib::RefPtr<void> handle) {
    
    if(user_prompt_handle) {
        //Still have open dialog
        //Respond with error.
        send_cancel();
    }

    sigc::slot<void(int)> user_response;

    switch(responseType) {
      case neon::USER_YN:
        user_response = sigc::mem_fun(*this, &Home::user_yes_no_response);
        break;
      case neon::USER_STRING:
        user_response = sigc::mem_fun(*this, &Home::user_string_response);
        break;
      case neon::USER_INT:
        user_response = sigc::mem_fun(*this, &Home::user_number_response);
        break;
      case neon::USER_NONE:
        user_response = sigc::mem_fun(*this, &Home::user_none_response);
        break;
    }

    window->showPopup(prompt, responseType, user_response);

    user_prompt_handle = handle;
    Logger::debug("Received prompt: " + prompt);
}

void Home::user_yes_no_response(int responseCode) {
    Logger::debug("Received response from user: " + std::to_string(responseCode));
    
    auto response = Glib::Variant<bool>::create(responseCode == Gtk::ResponseType::YES);
    window->hidePopup();
    auto hwif = window->hardwareInterface;
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

void Home::user_string_response(int responseCode) {
    if(responseCode == Gtk::ResponseType::CANCEL) {
        send_cancel();
        return;
    }

    window->hidePopup();
    auto ui = window->ui;
    auto text_input = ui->get_widget<Gtk::Entry>("text_user_input")->get_text(); 
    auto response = Glib::Variant<Glib::ustring>::create(text_input);
    Logger::debug("Received response from user: " + text_input);
    auto hwif = window->hardwareInterface;
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

void Home::user_number_response(int responseCode) {
    if(responseCode == Gtk::ResponseType::CANCEL) {
        send_cancel();
        return;
    }

    window->hidePopup();
    auto ui = window->ui;
    auto text_input = ui->get_widget<Gtk::Entry>("number_user_input")->get_text(); 
    auto response = Glib::Variant<guint32>::create(std::stoi(text_input));
    Logger::debug("Received response from user: " + text_input);
    auto hwif = window->hardwareInterface;
    hwif->respond_from_user(response, user_prompt_handle);
    user_prompt_handle.reset();
}

void Home::user_none_response(int responseCode) {
    window->hidePopup();
}
