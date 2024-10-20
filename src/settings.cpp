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

#include "settings.hpp"
#include "bluetooth-serial-port.hpp"
#include "dropdown.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include <cstddef>
#include <giomm/settings.h>
#include <glib.h>

// gtk.h provides GTK_INVALID_LIST_POSITION indirectly.
// direct inclusion of the header that provides it is not allowed.
#include <gtk/gtk.h> //NOLINT(misc-include-cleaner)
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/stringlist.h>
#include <memory>
#include <sigc++/functors/mem_fun.h>
#include <string>
#include <vector>

Settings::Settings(MainWindow* main_window)
    : window{main_window},
      visibleView{window->viewStack->property_visible_child_name()},
      btHardwareInterface{BluetoothSerialPort::get_BluetoothSerialPort()},
      serialHardwareInterface{main_window->serialPort}

{
    auto user_interface = window->ui;

    // Detect if view has changed.
    visibleView.signal_changed().connect(
        sigc::mem_fun(*this, &Settings::on_show));

    // Assign action to home button
    homeButton =
        user_interface->get_widget<Gtk::Button>("settings_home_button");
    homeButton->signal_clicked().connect(
        sigc::mem_fun(*this, &Settings::homeClicked));

    // Connection Settings
    // Assign actions to radio buttons
    bluetooth_rb =
        user_interface->get_widget<Gtk::CheckButton>("bluetooth_radio_button");
    serial_rb =
        user_interface->get_widget<Gtk::CheckButton>("serial_radio_button");
    bluetooth_rb->signal_toggled().connect(
        sigc::mem_fun(*this, &Settings::selectBluetooth));
    serial_rb->signal_toggled().connect(
        sigc::mem_fun(*this, &Settings::selectSerial));
    serial_rb->set_group(*bluetooth_rb); // link radio buttons together

    // Bluetooth specific options
    //---------------------------
    // Select host bluetooth adapter:

    btGrid = user_interface->get_widget<Gtk::Grid>("bluetooth_settings");
    btHostLabel = user_interface->get_widget<Gtk::Label>("host_label");
    m_bt_host_dropdown =
        user_interface->get_widget<Gtk::DropDown>("host_dropdown");
    m_bt_host_dropdown->property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &Settings::selectBluetoothController));

    // Select remote bluetooth device (OBD device):
    btDeviceLabel =
        user_interface->get_widget<Gtk::Label>("bluetooth_device_label");
    m_bt_device_dropdown = Gtk::Builder::get_widget_derived<Dropdown>(
        user_interface, "bluetooth_device_dropdown");
    m_bt_device_dropdown->property_selected_item().signal_changed().connect(
        sigc::mem_fun(*this, &Settings::selectBluetoothDevice));

    btScanLabel =
        user_interface->get_widget<Gtk::Label>("bluetooth_scan_label");
    btScanProgress =
        user_interface->get_widget<Gtk::ProgressBar>("bluetooth_scan_progress");
    btDeviceScan = user_interface->get_widget<Gtk::Button>("scan_bluetooth");
    btDeviceScan->signal_clicked().connect(
        sigc::mem_fun(*this, &Settings::scanBluetooth));

    // Serial port specific options
    serialGrid = user_interface->get_widget<Gtk::Grid>("serial_port_settings");

    // Load Settings
    settings = Gio::Settings::create("com.github.beardedone55.neonobd");
    iftype = static_cast<InterfaceType>(settings->get_enum("interface-type"));
    window->setHardwareInterface(iftype);
    if (iftype == neon::BLUETOOTH_IF) {
        bluetooth_rb->set_active();
    } else {
        serial_rb->set_active();
    }

    auto controllerName = settings->get_string("bluetooth-controller");
    if (!controllerName.empty()) {
        auto list_model = std::dynamic_pointer_cast<Gtk::StringList>(
            m_bt_host_dropdown->get_model());
        list_model->append(controllerName);
        m_bt_host_dropdown->set_selected(0);
    }

    auto deviceAddress = settings->get_string("selected-device-address");
    if (!deviceAddress.empty()) {
        auto deviceName = settings->get_string("selected-device-name");
        m_bt_device_dropdown->append(deviceAddress, deviceName);
        m_bt_device_dropdown->set_selected(0);
    }

    m_serial_device_dropdown =
        user_interface->get_widget<Gtk::DropDown>("serial_device");
    m_serial_baudrate_dropdown =
        user_interface->get_widget<Gtk::DropDown>("serial_baudrate");

    m_serial_device_dropdown->property_selected_item().signal_changed().connect(
        sigc::mem_fun(*this, &Settings::selectSerialDevice));
    m_serial_baudrate_dropdown->property_selected_item()
        .signal_changed()
        .connect(sigc::mem_fun(*this, &Settings::selectSerialBaudrate));

    populateDropdown(serialHardwareInterface->get_valid_baudrates(),
                     m_serial_baudrate_dropdown,
                     std::to_string(settings->get_enum("baud-rate")));

    Logger::debug("Created Settings object.");
}

Settings::~Settings() { Logger::debug("Settings object destroyed."); }

Glib::ustring Settings::getSelectedDevice() {
    auto ifType = settings->get_enum("interface-type");
    Glib::ustring selectedDevice;
    if (ifType == neon::BLUETOOTH_IF) {
        // Somebody wants the selected device in order to initiate a connection.
        // Make sure that the configured host adapter is selected.
        auto bluetoothController = settings->get_string("bluetooth-controller");
        btHardwareInterface->select_controller(bluetoothController);
        selectedDevice = settings->get_string("selected-device-address");
    } else {
        serialHardwareInterface->set_baudrate(
            std::to_string(settings->get_enum("baud-rate")));
        selectedDevice = settings->get_string("serial-port");
    }

    return selectedDevice;
}
void Settings::homeClicked() {
    window->viewStack->set_visible_child("home_view");
}

void Settings::selectBluetooth() {
    serialGrid->hide();
    btGrid->show();
    iftype = neon::BLUETOOTH_IF;
    if (settings->get_enum("interface-type") != neon::BLUETOOTH_IF) {
        settings->set_enum("interface-type", neon::BLUETOOTH_IF);
    }

    window->setHardwareInterface(neon::BLUETOOTH_IF);
}

void Settings::selectSerial() {
    btGrid->hide();
    serialGrid->show();
    iftype = neon::SERIAL_IF;
    if (settings->get_enum("interface-type") != neon::SERIAL_IF) {
        settings->set_enum("interface-type", neon::SERIAL_IF);
    }

    window->setHardwareInterface(neon::SERIAL_IF);
}

void Settings::selectBluetoothController() {
    // GTK_INVALID_LIST_POSITION is provided by gtk.h indirectly.
    // NOLINTNEXTLINE(misc-include-cleaner)
    if (m_bt_host_dropdown->get_selected() == GTK_INVALID_LIST_POSITION) {
        return;
    }

    auto list_model = std::dynamic_pointer_cast<Gtk::StringList>(
        m_bt_host_dropdown->get_model());
    auto new_bluetooth_controller =
        list_model->get_string(m_bt_host_dropdown->get_selected());
    settings->set_string("bluetooth-controller", new_bluetooth_controller);
    btHardwareInterface->select_controller(new_bluetooth_controller);
}

void Settings::selectBluetoothDevice() {
    auto activeDevice = m_bt_device_dropdown->get_active_values();
    if (activeDevice) {
        auto [address, name] = *activeDevice;
        Logger::debug("Settings: selected bluetooth device " + name);
        settings->set_string("selected-device-address", address);
        settings->set_string("selected-device-name", name);
    }
}

void Settings::selectSerialDevice() {
    // GTK_INVALID_LIST_POSITION is provided by gtk.h indirectly.
    // NOLINTNEXTLINE(misc-include-cleaner)
    if (m_serial_device_dropdown->get_selected() == GTK_INVALID_LIST_POSITION) {
        return;
    }

    auto list_model = std::dynamic_pointer_cast<Gtk::StringList>(
        m_serial_device_dropdown->get_model());
    auto selected_serial_device =
        list_model->get_string(m_serial_device_dropdown->get_selected());
    settings->set_string("serial-port", selected_serial_device);
}

void Settings::selectSerialBaudrate() {
    // GTK_INVALID_LIST_POSITION is provided by gtk.h indirectly.
    if (m_serial_baudrate_dropdown->get_selected() ==
        GTK_INVALID_LIST_POSITION) { // NOLINT(misc-include-cleaner)
        return;
    }

    auto list_model = std::dynamic_pointer_cast<Gtk::StringList>(
        m_serial_baudrate_dropdown->get_model());
    auto selected_baudrate =
        list_model->get_string(m_serial_baudrate_dropdown->get_selected());
    settings->set_enum("baud-rate", std::stoi(selected_baudrate));
}

void Settings::scanComplete() {
    auto& bluetooth = btHardwareInterface;

    // Update Dropdown
    auto devices = bluetooth->get_device_names_addresses();
    m_bt_device_dropdown->remove_all();

    auto defaultAddress = settings->get_string("selected-device-address");
    auto defaultName = settings->get_string("selected-device-name");
    if (!defaultAddress.empty()) {
        m_bt_device_dropdown->append(defaultAddress, defaultName);
    }

    for (auto& [address, name] : devices) {
        if (address != defaultAddress) {
            m_bt_device_dropdown->append(address, name);
        }
    }

    m_bt_device_dropdown->set_selected(0);

    btScanConnection.disconnect();

    // Hide Progress Bar
    btScanProgress->hide();
    btScanLabel->hide();

    // Show Device List
    btDeviceLabel->show();
    m_bt_device_dropdown->show();
    btDeviceScan->show();
}

void Settings::updateScanProgress(int percentComplete) {
    constexpr int SCAN_FINISHED = 100;
    if (percentComplete == SCAN_FINISHED) {
        scanComplete();
    } else {
        btScanProgress->set_fraction(static_cast<double>(percentComplete) /
                                     SCAN_FINISHED);
    }
}

void Settings::scanBluetooth() {
    auto& bluetooth = btHardwareInterface;
    btScanConnection = bluetooth->signal_probe_progress().connect(
        sigc::mem_fun(*this, &Settings::updateScanProgress));

    btScanProgress->set_fraction(0.0);
    btScanLabel->show();
    btScanProgress->show();

    btDeviceLabel->hide();
    m_bt_device_dropdown->hide();
    btDeviceScan->hide();

    bluetooth->probe_remote_devices();
}

void Settings::populateDropdown(const std::vector<Glib::ustring>& values,
                                Gtk::DropDown* dropdown,
                                const Glib::ustring& default_value) {

    auto list_model =
        std::dynamic_pointer_cast<Gtk::StringList>(dropdown->get_model());
    while (list_model->get_n_items() > 0) {
        list_model->remove(0);
    }
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& value = values[i];
        list_model->append(value);
        if (value == default_value) {
            // guint is provided by glib.h
            // NOLINTNEXTLINE(misc-include-cleaner)
            dropdown->set_selected(static_cast<guint>(i));
        }
    }
}

void Settings::on_show() {
    if (visibleView.get_value() != "settings_view") {
        return;
    }

    Logger::debug("Showing settings dialog.");
    // Populate Dropdown:
    auto controllers = btHardwareInterface->get_controller_names();
    populateDropdown(controllers, m_bt_host_dropdown,
                     settings->get_string("bluetooth-controller"));

    // If we only have one controller, asking user to select one is silly.
    if (controllers.size() == 1) {
        Logger::debug("Only one bluetooth controller found, hiding dropdown.");
        m_bt_host_dropdown->set_selected(0);
        btHostLabel->hide();
        m_bt_host_dropdown->hide();
    } else {
        Logger::debug("Showing bluetooth controller dropdown, " +
                      Glib::ustring::format(controllers.size()) + "found.");
        btHostLabel->show();
        m_bt_host_dropdown->show();
    }

    // No bluetooth available...
    if (controllers.empty()) {
        serial_rb->set_active();
        bluetooth_rb->set_sensitive(false); // Disable Bluetooth
    } else {
        bluetooth_rb->set_sensitive(true); // Enable Bluetooth
    }

    // Populate comboboxes for serial port

    populateDropdown(SerialPort::get_serial_devices(), m_serial_device_dropdown,
                     settings->get_string("serial-port"));
}
