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

#include "mainwindow.hpp"
#include "bluetooth-serial-port.hpp"
#include "home.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include "settings.hpp"
#include "terminal.hpp"
#include "ui_neonobd.h"
#include <memory>

MainWindow::MainWindow()
    : m_view_stack(this), m_home(this), m_settings(this),
      m_terminal(this) {

    m_ui.setupUi(&m_view_stack);

    m_home = std::make_unique<Home>(this);
    m_settings = std::make_unique<Settings>(this);
    m_terminal = std::make_unique<Terminal>(this);
}

MainWindow::~MainWindow() { Logger::debug("Destroying MainWindow."); }

void MainWindow::setHardwareInterface(InterfaceType ifType) {
    switch (ifType) {
    case neon::BLUETOOTH_IF:
        m_hardware_interface = &m_bluetooth_serial_port;
        break;
    case neon::SERIAL_IF:
        m_hardware_interface = &m_serial_port;
        break;
    }
}

int MainWindow::user_get_int(const QString& prompt, bool& ok) {
    return QInputDialog::getInt(nullptr, "", prompt, QLineEdit::Normal,
                                            "", &ok);
}

QString MainWindow::user_get_text(const QString& prompt, bool& ok) {
    return QInputDialog::getText(nullptr, "", prompt, QLineEdit::Normal,
                                            "", &ok);
}

bool MainWindow::user_get_yes_no(const QString& prompt) {
    return QMessageBox::question(nullptr, "", prompt) == QMessageBox::Yes;
}
