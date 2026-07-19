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
#include "hardware-interface.hpp"
#include "home.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include "settings.hpp"
#include "terminal.hpp"
#include "ui_neonobd.h"
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QString>
#include <QVBoxLayout>
#include <climits>

MainWindow::MainWindow()
    : m_home(this), m_settings(this), m_terminal(this), m_ui{},
      m_bluetooth_socket_notifier(m_bluetooth_serial_port.get_event_fd(),
                                  QSocketNotifier::Read, this),
      m_serial_socket_notifier(m_serial_port.get_event_fd(),
                               QSocketNotifier::Read, this),
      m_window_layout(this), m_view_stack(this) {

    m_ui.setupUi(&m_view_stack);
    m_home.init();
    m_settings.init();
    m_terminal.init();
    connect(&m_bluetooth_socket_notifier, &QSocketNotifier::activated, this,
            &MainWindow::process_bluetooth_events);
    connect(&m_serial_socket_notifier, &QSocketNotifier::activated, this,
            &MainWindow::process_serial_port_events);
    m_bluetooth_serial_port.process_events();

    m_window_layout.addWidget(&m_view_stack);

    Logger::debug << "Created MainWindow\n";
}

MainWindow::~MainWindow() { Logger::debug("Destroying MainWindow."); }

void MainWindow::set_hardware_interface(InterfaceType if_type) {
    switch (if_type) {
    case neon::BLUETOOTH_IF:
        m_hardware_interface = &m_bluetooth_serial_port;
        break;
    case neon::SERIAL_IF:
        m_hardware_interface = &m_serial_port;
        break;
    }
}

int MainWindow::user_get_int(const QString& prompt, bool& ok_clicked) {
    return QInputDialog::getInt(nullptr, "", prompt, 0, INT_MIN, INT_MAX, 1,
                                &ok_clicked);
}

QString MainWindow::user_get_text(const QString& prompt, bool& ok_clicked) {
    return QInputDialog::getText(nullptr, "", prompt, QLineEdit::Normal, "",
                                 &ok_clicked);
}

bool MainWindow::user_get_yes_no(const QString& prompt) {
    return QMessageBox::question(nullptr, "", prompt) == QMessageBox::Yes;
}

Ui::ViewStack& MainWindow::get_ui() { return m_ui; }

QStackedWidget& MainWindow::get_view_stack() { return m_view_stack; }

BluetoothSerialPort& MainWindow::get_bt_serial_port() {
    return m_bluetooth_serial_port;
}

SerialPort& MainWindow::get_serial_port() { return m_serial_port; }

HardwareInterface* MainWindow::get_hardware_interface() {
    return m_hardware_interface;
}

void MainWindow::process_bluetooth_events(QSocketDescriptor /*unused*/,
                                          QSocketNotifier::Type /*unused*/) {
    m_bluetooth_serial_port.process_events();
}

void MainWindow::process_serial_port_events(QSocketDescriptor /*unused*/,
                                            QSocketNotifier::Type /*unused*/) {
    m_serial_port.process_events();
}
