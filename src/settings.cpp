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

#include "settings.hpp"
#include "bluetooth-serial-port.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include "ui_neonobd.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

Settings::Settings(MainWindow* main_window)
    : m_window{main_window},
      m_bt_hardware_interface{main_window->get_bt_serial_port()},
      m_serial_hardware_interface{main_window->get_serial_port()},
      m_settings{"beardedone55","neonobd"}

{}

void Settings::init() {
    auto& user_interface = m_window->get_ui();

    // Detect if view has changed.
    connect(&m_window->get_view_stack(), &QStackedWidget::currentChanged,
            this, &Settings::on_show);

    // Assign action to home button
    m_home_button = user_interface.settings_home_button;
    connect(m_home_button, &QPushButton::clicked, this, &Settings::home_clicked);

    // Connection Settings
    // Assign actions to radio buttons

    m_bluetooth_rb = user_interface.bluetooth_rb;
    connect(m_bluetooth_rb, &QRadioButton::clicked, this, &Settings::select_bluetooth);

    m_serial_rb = user_interface.serial_rb;
    connect(m_serial_rb, &QRadioButton::clicked, this, &Settings::select_serial);


    // Bluetooth specific options
    //---------------------------
    // Select host bluetooth adapter:

    m_bt_grid = user_interface.bluetooth_settings;
    m_host_label = user_interface.host_label;
    m_bt_host_dropdown = user_interface.bluetooth_host_controller;
    connect(m_bt_host_dropdown, &QComboBox::currentIndexChanged, 
            this, &Settings::select_bluetooth_controller);

    // Select remote bluetooth device (OBD device):
    m_bt_device_label = user_interface.bluetooth_device_label;
    m_bt_device_dropdown = user_interface.bluetooth_OBD_device;
    connect(m_bt_device_dropdown, &QComboBox::currentIndexChanged,
            this, &Settings::select_bluetooth_device);

    m_bt_scan_label = user_interface.bluetooth_scan_label;
    m_bt_scan_progress = user_interface.bluetooth_scan_progress;
    m_bt_device_scan = user_interface.bluetooth_scan;
    connect(m_bt_device_scan, &QPushButton::clicked, this, &Settings::scan_bluetooth);

    // Serial port specific options

    m_serial_grid = user_interface.serial_port_settings;

    m_serial_device_dropdown = user_interface.serial_port_device;
    m_serial_baudrate_dropdown = user_interface.serial_port_baudrate;
    connect(m_serial_device_dropdown, &QComboBox::currentIndexChanged,
            this, &Settings::select_serial_device);
    connect(m_serial_baudrate_dropdown, &QComboBox::currentIndexChanged,
            this, &Settings::select_serial_baudrate);

    // Load Settings
    m_iftype = static_cast<InterfaceType>(m_settings.value("interface-type",0).toInt());
    switch(m_iftype) {
        case neon::SERIAL_IF:
           m_serial_rb->setChecked(true);
           select_serial();
           break;
        case neon::BLUETOOTH_IF:
        default:
           m_bluetooth_rb->setChecked(true);
           select_bluetooth();
           break;
    }
    m_window->set_hardware_interface(m_iftype);

    auto controller_name = m_settings.value("bluetooth-controller").toString();
    if (!controller_name.isEmpty()) {
        m_bt_host_dropdown->addItem(controller_name);
        m_bt_host_dropdown->setCurrentIndex(0);
    }

    auto device_address = m_settings.value("selected-device-address").toString();
    if (!device_address.isEmpty()) {
        add_device(m_settings.value("selected-device-name").toString(), device_address);
        m_bt_device_dropdown->setCurrentIndex(0);
    }


    populate_dropdown(m_serial_hardware_interface.get_valid_baudrates(),
                     m_serial_baudrate_dropdown,
                     m_settings.value("baud-rate","9600").toString());

    Logger::debug("Created Settings object.");
}

Settings::~Settings() { Logger::debug("Settings object destroyed."); }

void Settings::add_device(const QString& name, const QString& address) {
    m_bt_device_dropdown->addItem(name + "|<" + address + ">");
}

QString Settings::get_selected_device() {
    auto if_type = m_settings.value("interface-type",0).toInt();
    QString selected_device;
    if (if_type == neon::BLUETOOTH_IF) {
        // Somebody wants the selected device in order to initiate a connection.
        // Make sure that the configured host adapter is selected.
        auto bluetooth_controller = m_settings.value("bluetooth-controller").toString();
        m_bt_hardware_interface.select_controller(bluetooth_controller.toStdString());
        selected_device = m_settings.value("selected-device-address").toString();
    } else {
        m_serial_hardware_interface.set_baudrate(
            m_settings.value("baud-rate").toString().toStdString());
        selected_device = m_settings.value("serial-port").toString();
    }

    return selected_device;
}
void Settings::home_clicked() {
    QWidget* home_view = m_window->get_ui().home_view;
    m_window->get_view_stack().setCurrentWidget(home_view);
}

void Settings::select_bluetooth() {
    m_serial_grid->hide();
    m_bt_grid->show();
    m_iftype = neon::BLUETOOTH_IF;
    if (m_settings.value("interface-type").toInt() != neon::BLUETOOTH_IF) {
        m_settings.setValue("interface-type", neon::BLUETOOTH_IF);
    }

    m_window->set_hardware_interface(neon::BLUETOOTH_IF);
}

void Settings::select_serial() {
    m_bt_grid->hide();
    m_serial_grid->show();
    m_iftype = neon::SERIAL_IF;
    if (m_settings.value("interface-type").toInt() != neon::SERIAL_IF) {
        m_settings.setValue("interface-type", neon::SERIAL_IF);
    }

    m_window->set_hardware_interface(neon::SERIAL_IF);
}

bool Settings::valid_dropdown_index(int index, QComboBox* dropdown) {
    return index >= 0 && index < dropdown->count();
}

void Settings::select_bluetooth_controller(int index) {
    Logger::debug << "select_bluetooth_controller called for index " << index << ".\n";
    if (!valid_dropdown_index(index, m_bt_host_dropdown)) {
        return;
    }

    Logger::debug << "Setting bluetooth controller.\n";
    auto new_bluetooth_controller = m_bt_host_dropdown->currentText();
    m_settings.setValue("bluetooth-controller", new_bluetooth_controller);
    m_bt_hardware_interface.select_controller(new_bluetooth_controller.toStdString());
}

void Settings::select_bluetooth_device(int index) {
    if (!valid_dropdown_index(index, m_bt_device_dropdown)) {
        return;
    }
    auto address_and_name = m_bt_device_dropdown->currentText().split('|');
    auto name = address_and_name[0];
    auto address = address_and_name[1].split('<')[1].split('>')[0];
    if (!address.isEmpty()) {
        Logger::debug << "Settings: selected bluetooth device " << name.toStdString() << "\n";
        m_settings.setValue("selected-device-address", address);
        m_settings.setValue("selected-device-name", name);
    }
}

void Settings::select_serial_device(int index) {
    if (!valid_dropdown_index(index, m_serial_device_dropdown)) {
        return;
    }

    auto selected_serial_device = m_serial_device_dropdown->currentText();
    m_settings.setValue("serial-port", selected_serial_device);
}

void Settings::select_serial_baudrate(int index) {
    if (!valid_dropdown_index(index, m_serial_baudrate_dropdown)) {
        return;
    }
    auto selected_baudrate = m_serial_baudrate_dropdown->currentText();
    m_settings.setValue("baud-rate", selected_baudrate.toInt());
}

void Settings::scan_complete() {
    auto& bluetooth = m_bt_hardware_interface;

    // Update Dropdown
    auto devices = bluetooth.get_device_names_addresses();
    m_bt_device_dropdown->clear();

    auto default_address = m_settings.value("selected-device-address").toString();
    auto default_name = m_settings.value("selected-device-name").toString();
    if (!default_address.isEmpty()) {
        add_device(default_name, default_address);
    }

    for (auto& device : devices) {
        if (device.address != default_address.toStdString()) {
            add_device(QString::fromStdString(device.name), QString::fromStdString(device.address));
        }
    }

    m_bt_device_dropdown->setCurrentIndex(0);

//    btScanConnection.disconnect();

    // Hide Progress Bar
    m_bt_scan_progress->hide();
    m_bt_scan_label->hide();

    // Show Device List
    m_bt_device_label->show();
    m_bt_device_dropdown->show();
    m_bt_device_scan->show();
}

void Settings::update_scan_progress(int percent_complete) {
    constexpr int SCAN_FINISHED = 100;
    if (percent_complete == SCAN_FINISHED) {
        scan_complete();
    } else {
        m_bt_scan_progress->setValue(percent_complete);
    }
}

void Settings::scan_bluetooth() {
    auto& bluetooth = m_bt_hardware_interface;
    
    m_bt_scan_progress->setValue(0);
    m_bt_scan_label->show();
    m_bt_scan_progress->show();

    m_bt_device_label->hide();
    m_bt_device_dropdown->hide();
    m_bt_device_scan->hide();

    bluetooth.probe_remote_devices(
        [this](int percent_complete){update_scan_progress(percent_complete);});
}

void Settings::populate_dropdown(const std::vector<std::string>& values,
                                QComboBox* dropdown,
                                const QString& default_value) {

    dropdown->clear();
    for (const auto& value : values) {
        dropdown->addItem(QString::fromStdString(value));    
    }
    dropdown->setCurrentText(default_value);
}

void Settings::on_show() {
    if (m_window->get_view_stack().currentWidget() != 
        m_window->get_ui().settings_view) {
        return;
    }

    Logger::debug("Showing settings dialog.");
    // Populate Dropdown:
    auto controllers = m_bt_hardware_interface.get_controller_names();
    populate_dropdown(controllers, m_bt_host_dropdown,
                     m_settings.value("bluetooth-controller").toString());

    // If we only have one controller, asking user to select one is silly.
    if (controllers.size() == 1) {
        Logger::debug("Only one bluetooth controller found, disabling dropdown.");
        m_bt_host_dropdown->setCurrentIndex(0);
        m_bt_host_dropdown->setEnabled(false);
    } else {
        Logger::debug << "Showing bluetooth controller dropdown, " <<
                      controllers.size() << " found.\n";
        m_bt_host_dropdown->setEnabled(true);
    }

    // No bluetooth available...
    if (controllers.empty()) {
        m_serial_rb->setChecked(true);
        select_serial();
        m_bluetooth_rb->setEnabled(false); // Disable Bluetooth
    } else {
        m_bluetooth_rb->setEnabled(true); // Enable Bluetooth
    }

    // Populate comboboxes for serial port

    populate_dropdown(SerialPort::get_serial_devices(), m_serial_device_dropdown,
                     m_settings.value("serial-port").toString());
}
