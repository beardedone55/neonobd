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

#include "settings.h"
#include "logger.h"
#include "mainwindow.h"

Settings::Settings(MainWindow* main_window) :
   window{main_window},
   visibleView{window->viewStack->property_visible_child_name()},
   btHardwareInterface{BluetoothSerialPort::getBluetoothSerialPort()}

{
    auto ui = window->ui;

    serialHardwareInterface = window->serialPort;

    //Detect if view has changed.
    visibleView.signal_changed().connect(sigc::mem_fun(*this, &Settings::on_show));

    //Assign action to home button
    homeButton = ui->get_widget<Gtk::Button>("settings_home_button");
    homeButton->signal_clicked().connect(sigc::mem_fun(*this, &Settings::homeClicked));

    //Connection Settings
    //Assign actions to radio buttons
    bluetooth_rb = ui->get_widget<Gtk::CheckButton>("bluetooth_radio_button");
    serial_rb = ui->get_widget<Gtk::CheckButton>("serial_radio_button");
    bluetooth_rb->signal_toggled().connect(sigc::mem_fun(*this, &Settings::selectBluetooth));
    serial_rb->signal_toggled().connect(sigc::mem_fun(*this,&Settings::selectSerial));
    serial_rb->set_group(*bluetooth_rb);  //link radio buttons together

    //Bluetooth specific options
    //---------------------------
    //Select host bluetooth adapter:
    
    btGrid = ui->get_widget<Gtk::Grid>("bluetooth_settings");
    btHostLabel = ui->get_widget<Gtk::Label>("host_label");
    btHostCombo = ui->get_widget<Gtk::ComboBoxText>("host_combo");
    btHostCombo->signal_changed().connect(sigc::mem_fun(*this,&Settings::selectBluetoothController));

    //Select remote bluetooth device (OBD device):
    btDeviceLabel = ui->get_widget<Gtk::Label>("bluetooth_device_label");
    btDeviceCombo = Gtk::Builder::get_widget_derived<ComboBox<Glib::ustring, Glib::ustring>>(ui, "bluetooth_device_combo");
    btDeviceCombo->signal_changed().connect(sigc::mem_fun(*this,&Settings::selectBluetoothDevice));

    btScanLabel = ui->get_widget<Gtk::Label>("bluetooth_scan_label");
    btScanProgress = ui->get_widget<Gtk::ProgressBar>("bluetooth_scan_progress");
    btDeviceScan = ui->get_widget<Gtk::Button>("scan_bluetooth");
    btDeviceScan->signal_clicked().connect(sigc::mem_fun(*this,&Settings::scanBluetooth));

    
    //Serial port specific options
    serialGrid = ui->get_widget<Gtk::Grid>("serial_port_settings");

    //Load Settings
    settings = Gio::Settings::create("com.github.beardedone55.neonobd");
    iftype = static_cast<InterfaceType>(settings->get_enum("interface-type"));
    window->setHardwareInterface(iftype);
    if(iftype == neon::BLUETOOTH_IF) {
        bluetooth_rb->set_active();
    } else {
        serial_rb->set_active();
    }

    auto controllerName = settings->get_string("bluetooth-controller");
    if(controllerName != "")
    {
        btHostCombo->append(controllerName);
        btHostCombo->set_active(0);
    }

    auto deviceAddress = settings->get_string("selected-device-address");
    if(deviceAddress != "")
    {
        auto deviceName = settings->get_string("selected-device-name");
        btDeviceCombo->append(deviceAddress, deviceName);
        btDeviceCombo->set_active(0);
    }

    serialDeviceCombo = ui->get_widget<Gtk::ComboBoxText>("serial_device");
    serialBaudrateCombo = ui->get_widget<Gtk::ComboBoxText>("serial_baudrate");

    serialDeviceCombo->signal_changed().connect(sigc::mem_fun(*this,&Settings::selectSerialDevice));
    serialBaudrateCombo->signal_changed().connect(sigc::mem_fun(*this,&Settings::selectSerialBaudrate));

    populateComboBox(SerialPort::get_valid_baudrates(), serialBaudrateCombo,
                     std::to_string(settings->get_enum("baud-rate")));

    Logger::debug("Created Settings object.");
}

Settings::~Settings() {
    Logger::debug("Settings object destroyed.");
}

Glib::ustring Settings::getSelectedDevice() {
    auto ifType = settings->get_enum("interface-type");
    Glib::ustring selectedDevice;
    if(ifType == neon::BLUETOOTH_IF) {
        //Somebody wants the selected device in order to initiate a connection.
        //Make sure that the configured host adapter is selected.
        auto bluetoothController = settings->get_string("bluetooth-controller");
        btHardwareInterface->select_controller(bluetoothController);
        selectedDevice = settings->get_string("selected-device-address");
    } else {
        serialHardwareInterface->set_baudrate(std::to_string(settings->get_enum("baud-rate")));
        selectedDevice = settings->get_string("serial-port");
    }

    return selectedDevice;
}
void Settings::homeClicked() {
    window->viewStack->set_visible_child("home_view");
}

void Settings::selectBluetooth()
{
    serialGrid->hide();
    btGrid->show();
    iftype = neon::BLUETOOTH_IF;
    if(settings->get_enum("interface-type") != neon::BLUETOOTH_IF)
        settings->set_enum("interface-type", neon::BLUETOOTH_IF);

    window->setHardwareInterface(neon::BLUETOOTH_IF);
}

void Settings::selectSerial()
{
    btGrid->hide();
    serialGrid->show();
    iftype = neon::SERIAL_IF;
    if(settings->get_enum("interface-type") != neon::SERIAL_IF)
        settings->set_enum("interface-type", neon::SERIAL_IF);

    window->setHardwareInterface(neon::SERIAL_IF);
}

void Settings::selectBluetoothController()
{
    if(btHostCombo->get_active_row_number() == -1)
        return;

    auto newBluetoothController = btHostCombo->get_active_text();
    settings->set_string("bluetooth-controller", newBluetoothController);
    btHardwareInterface->select_controller(newBluetoothController);
}

void Settings::selectBluetoothDevice()
{
    auto activeDevice = btDeviceCombo->get_active_values();
    if(activeDevice)
    {
        auto [ address, name ] = *activeDevice;
        Logger::debug("Settings: selected bluetooth device " + name);
        settings->set_string("selected-device-address", address);
        settings->set_string("selected-device-name", name);
    }
}

void Settings::selectSerialDevice() {
    if(serialDeviceCombo->get_active_row_number() == -1)
        return;

    auto selectedSerialDevice = serialDeviceCombo->get_active_text();
    settings->set_string("serial-port", selectedSerialDevice);
}

void Settings::selectSerialBaudrate() {
    if(serialBaudrateCombo->get_active_row_number() == -1)
        return;

    auto selectedBaudrate = serialBaudrateCombo->get_active_text();
    settings->set_enum("baud-rate", std::stoi(selectedBaudrate));
}


void Settings::scanComplete()
{
    auto& bt = btHardwareInterface;

    //Update Combo Box
    auto devices = bt->get_device_names_addresses();
    btDeviceCombo->remove_all();

    auto defaultAddress = settings->get_string("selected-device-address");
    auto defaultName = settings->get_string("selected-device-name");
    if (defaultAddress != "")
        btDeviceCombo->append(defaultAddress, defaultName);

    for (auto &[address, name] : devices)
        if(address != defaultAddress)
            btDeviceCombo->append(address, "<" + name + ">");

    btDeviceCombo->set_active(0);

    btScanConnection.disconnect();
    
    //Hide Progress Bar
    btScanProgress->hide();
    btScanLabel->hide();

    //Show Device List
    btDeviceLabel->show();
    btDeviceCombo->show();
    btDeviceScan->show();
}

void Settings::updateScanProgress(int percentComplete)
{
    if(percentComplete == 100)
        scanComplete();
    else
        btScanProgress->set_fraction(percentComplete / 100.0);
}

void Settings::scanBluetooth()
{
    auto& bt = btHardwareInterface;
    btScanConnection = bt->signal_probe_progress().connect(sigc::mem_fun(*this,&Settings::updateScanProgress));

    btScanProgress->set_fraction(0.0);
    btScanLabel->show();
    btScanProgress->show();
    
    btDeviceLabel->hide();
    btDeviceCombo->hide();
    btDeviceScan->hide();

    bt->probe_remote_devices();
}

void Settings::populateComboBox(const std::vector<Glib::ustring>& values,
                                Gtk::ComboBoxText* combobox,
                                const Glib::ustring& default_value)
{
    combobox->remove_all();
    for(unsigned int i=0; i < values.size(); ++i) {
        auto& value = values[i];
        combobox->append(value);
        if(value == default_value) {
            combobox->set_active(i);
        }
    }
}

void Settings::on_show()
{
    if(visibleView.get_value() != "settings_view")
        return;

    Logger::debug("Showing settings dialog.");
    //Populate Combo Box:
    auto controllers = btHardwareInterface->get_controller_names();
    populateComboBox(controllers, btHostCombo, settings->get_string("bluetooth-controller"));

    //If we only have one controller, asking user to select one is silly.
    if(controllers.size() == 1) {
        Logger::debug("Only one bluetooth controller found, hiding dropdown.");
        btHostCombo->set_active(0);
        btHostLabel->hide();
        btHostCombo->hide();
    } else {
        Logger::debug("Showing bluetooth controller dropdown, " + Glib::ustring::format(controllers.size()) + "found.");
        btHostLabel->show();
        btHostCombo->show();
    }

    //No bluetooth available...
    if(controllers.size() == 0) {
        serial_rb->set_active();
        bluetooth_rb->set_sensitive(false); //Disable Bluetooth
    } else {
        bluetooth_rb->set_sensitive(true); //Enable Bluetooth
    }

    //Populate comboboxes for serial port

    populateComboBox(SerialPort::get_serial_devices(), serialDeviceCombo, settings->get_string("serial-port"));
}
