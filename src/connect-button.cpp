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

#include "connect-button.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"
#include "neonobd_types.hpp"

#include <QPushButton>
#include <QString>
#include <QWidget>
#include <string>

ConnectButton::ConnectButton(QWidget* parent) : QPushButton{parent} {
    connect(this, &QPushButton::clicked, this, &ConnectButton::on_clicked);
}

void ConnectButton::on_clicked() {
    Logger::debug("Connect button clicked.");
    auto* main_window = dynamic_cast<MainWindow*>(window());
    auto* hwif = main_window->get_hardware_interface();

    main_window->m_home.disable_all();

    hwif->connect_user_input(
        [this](const std::string& text, const ResponseType response_type,
               void* handle) { user_prompt(text, response_type, handle); });

    if (!hwif->connect(
            main_window->m_settings.get_selected_device().toStdString(),
            [this](bool success) { connect_complete(success); })) {
        connect_complete(false);
    }
}

void ConnectButton::connect_complete(bool result) {
    auto* main_window = dynamic_cast<MainWindow*>(window());
    auto* hwif = main_window->get_hardware_interface();
    hwif->disconnect_user_input();
    main_window->m_home.enable_all();

    if (result) {
        Logger::debug("Connection to device was successful!");
        main_window->m_home.set_connected(true);
    } else {
        Logger::debug("Connection to device failed!");
    }
}

void ConnectButton::send_cancel(void* handle) {
    auto* hwif = dynamic_cast<MainWindow*>(window())->get_hardware_interface();
    Logger::debug("Responding with cancel from user.");
    hwif->respond_from_user(false, handle);
}

void ConnectButton::user_prompt(const std::string& prompt,
                                ResponseType responseType, void* handle) {

    const QString q_prompt = QString::fromStdString(prompt);
    switch (responseType) {
    case neon::USER_YN:
        user_yes_no_response(q_prompt, handle);
        break;
    case neon::USER_STRING:
        user_text_response(q_prompt, handle);
        break;
    case neon::USER_INT:
        user_number_response(q_prompt, handle);
        break;
    default:
        break;
    }

    Logger::debug("Received prompt: " + prompt);
}

void ConnectButton::user_yes_no_response(const QString& prompt, void* handle) {
    auto* main_window = dynamic_cast<MainWindow*>(window());
    bool response = MainWindow::user_get_yes_no(prompt);

    Logger::debug << "Received response from user: "
                  << (response ? "Yes" : "No") << "\n";

    auto* hwif = main_window->get_hardware_interface();
    hwif->respond_from_user(response, handle);
}

void ConnectButton::user_text_response(const QString& prompt, void* handle) {
    bool ok_clicked = false;
    auto* main_window = dynamic_cast<MainWindow*>(window());
    auto text_input = MainWindow::user_get_text(prompt, ok_clicked);
    if (ok_clicked && !text_input.isEmpty()) {
        std::string response = text_input.toStdString();
        Logger::debug << "Received response from user: "
                      << text_input.toStdString() << "\n";
        main_window->get_hardware_interface()->respond_from_user(response,
                                                                 handle);
    } else if (!ok_clicked) {
        send_cancel(handle);
    }
}

void ConnectButton::user_number_response(const QString& prompt, void* handle) {
    bool ok_clicked = false;
    auto* main_window = dynamic_cast<MainWindow*>(window());
    auto response = MainWindow::user_get_int(prompt, ok_clicked);
    if (ok_clicked) {
        Logger::debug("Received response from user: " +
                      std::to_string(response));
        main_window->get_hardware_interface()->respond_from_user(response,
                                                                 handle);
    } else {
        send_cancel(handle);
    }
}
