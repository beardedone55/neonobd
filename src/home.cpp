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

#include "home.hpp"
#include "connect-button.hpp"
#include "mainwindow.hpp"
#include <QPushButton>

Home::Home(MainWindow* main_window) : m_window{main_window} {}

void Home::init() {
    const auto& user_interface = m_window->get_ui();
    // Settings button
    m_settings_btn = user_interface.settings_button;
    connect(m_settings_btn, &QPushButton::clicked, this,
            &Home::settings_clicked);
    m_enabled_buttons.insert(m_settings_btn);

    // Connect button
    m_connect_btn = user_interface.connect_button;
    m_enabled_buttons.insert(m_connect_btn);

    // Terminal button
    m_terminal_btn = user_interface.terminal_button;
    m_terminal_btn->setEnabled(false);
    connect(m_terminal_btn, &QPushButton::clicked, this,
            &Home::terminal_clicked);
}

void Home::settings_clicked() {
    auto* settings_view = m_window->get_ui().settings_view;
    m_window->get_view_stack().setCurrentWidget(settings_view);
}

void Home::terminal_clicked() {
    auto* terminal_view = m_window->get_ui().terminal_view;
    m_window->get_view_stack().setCurrentWidget(terminal_view);
}

void Home::disable_all() {
    while (!m_enabled_buttons.empty()) {
        disable_button(*m_enabled_buttons.begin());
    }
}

void Home::enable_all() {
    enable_button(m_settings_btn);
    enable_button(m_connect_btn);
}

void Home::set_connected(bool isConnected) {
    m_connected = isConnected;

    if (m_connected) {
        disable_button(m_connect_btn);
        enable_button(m_terminal_btn);
    } else {
        enable_button(m_connect_btn);
        disable_button(m_terminal_btn);
    }
}

void Home::disable_button(QPushButton* button) {
    if (m_enabled_buttons.contains(button)) {
        button->setEnabled(false);
        m_enabled_buttons.erase(button);
    }
}

void Home::enable_button(QPushButton* button) {
    button->setEnabled(true);
    m_enabled_buttons.insert(button);
}
