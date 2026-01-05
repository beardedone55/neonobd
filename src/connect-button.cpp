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

#include <string>
#include <utility>
#include <QPushButton>
#include <QVariant>
#include <QString>

ConnectButton::ConnectButton()
	: window{std::dynamic_cast<MainWindow>(window())}
{
	connect(this, &QPushButton::clicked, this &ConnectButton::on_clicked);
}

void ConnectButton::on_clicked() {
    Logger::debug("Connect button clicked.");
    auto hwif = window->hardwareInterface;

    window->home.disable_all();

    connect(hwif, &HardwareInterface::request_user_input,
            this, &ConnectButton::user_prompt);

    connect(hwif, &HardwareInterface::complete_connection,
            this, &ConnectButton::connect_complete);

    if (!hwif->connect(window->settings.getSelectedDevice())) {
        connect_complete(false);
    }
}

void ConnectButton::connect_complete(bool result) {
    disconnect(hwif, &HardwareInterface::complete_connection,
               this, &ConnectButton::user_prompt);
    disconnect(hwif, &HardwareInterface::complete_connection,
               this, &ConnectButton::connect_complete);

    if (result) {
        Logger::debug("Connection to device was successful!");
        window->home.set_connected(true);
    } else {
        Logger::debug("Connection to device failed!");
    }

    window->home.enable_all();
}

void ConnectButton::send_cancel(std::shared_ptr<void> handle) {
    auto hwif = window->hardwareInterface;
    Logger::debug("Responding with cancel from user.");
    auto response = QVariant<bool>(false);
    hwif->respond_from_user(response, handle);
}

void ConnectButton::user_prompt(const QString& prompt,
                                ResponseType responseType,
                                std::shared_ptr<void> handle) {

    switch (responseType) {
    case neon::USER_YN:
        user_yes_no_response(prompt, handle);
        break;
    case neon::USER_STRING:
        user_text_response(prompt, handle);
        break;
    case neon::USER_INT:
        user_number_response(prompt, handle);
        break;
    default:
        break;
    }

    Logger::debug("Received prompt: " + prompt);
}

void ConnectButton::user_yes_no_response(
        const QString& prompt,
        std::shared_ptr<void> handle) {
    bool response = window->user_get_yes_no(prompt);

    Logger::debug("Received response from user: " +
                  response ? "Yes" : "No");

    auto hwif = window->hardwareInterface;
    hwif->respond_from_user(QVariant<bool>(response),
                            handle);
}

void ConnectButton::user_text_response(const QString& prompt,
                                       std::shared_ptr<void> handle) {
    bool ok;
    auto text_input = window->user_get_text(prompt, ok);
    if (ok && !text_input.empty()) {
        auto response = QVariant<QString>(text_input);
        Logger::debug("Received response from user: " + text_input);
        window->hardwareInterface->respond_from_user(response,
                                                     user_prompt_handle);
    } else if (!ok) {
        send_cancel(handle);
    }
}

void ConnectButton::user_number_response(const QString& prompt,
                                         std::shared_prt<void> handle) {
    bool ok;
    auto response = window->user_get_int(prompt, ok);
    if (ok) {
        Logger::debug("Received response from user: " + 
                       std::to_string(response));
        window->hardwareInterface->respond_from_user(
                QVariant<int>(response), handle);
    } else {
        send_cancel(handle);
    }
}

