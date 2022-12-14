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

Settings::Settings(const Glib::RefPtr<Gtk::Builder>& ui, Gtk::Stack* viewStack) :
   ui{ui},
   viewStack{viewStack},
   visibleView{viewStack->property_visible_child_name()}
{
    btHardwareInterface = BluetoothSerialPort::getBluetoothSerialPort();

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
    if(iftype == BLUETOOTH_IF)
        bluetooth_rb->set_active();
    else
        serial_rb->set_active();

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
    Logger::debug("Created Settings object.");
}

Settings::~Settings() {
    Logger::debug("Settings object destroyed.");
}


Settings::InterfaceType Settings::getInterfaceType() {
    return iftype;
}

void Settings::homeClicked() {
    viewStack->set_visible_child("home_view");
}

void Settings::selectBluetooth()
{
    serialGrid->hide();
    btGrid->show();
    iftype = BLUETOOTH_IF;
    if(settings->get_enum("interface-type") != BLUETOOTH_IF)
        settings->set_enum("interface-type", BLUETOOTH_IF);
}

void Settings::selectSerial()
{
    btGrid->hide();
    serialGrid->show();
    iftype = SERIAL_IF;
    if(settings->get_enum("interface-type") != SERIAL_IF)
        settings->set_enum("interface-type", SERIAL_IF);
}

void Settings::selectBluetoothController()
{
    if(btHostCombo->get_active_row_number() == -1)
        return;

    Logger::debug("Selecting new bluetooth controller.");
    auto bluetoothController = settings->get_string("bluetooth-controller");
    Logger::debug("Previous controller: " + bluetoothController);
    auto newBluetoothController = btHostCombo->get_active_text();
    Logger::debug("Settings: selected bluetooth controller " + newBluetoothController);
    if(newBluetoothController != bluetoothController)
    {
        settings->set_string("bluetooth-controller", newBluetoothController);
    }
    btHardwareInterface->select_controller(newBluetoothController);
}

void Settings::selectBluetoothDevice()
{
    auto defaultAddress = settings->get_string("selected-device-address");
    auto activeDevice = btDeviceCombo->get_active_values();
    if(activeDevice)
    {
        auto [ address, name ] = *activeDevice;
        Logger::debug("Settings: selected bluetooth device " + name);
        settings->set_string("selected-device-address", address);
        settings->set_string("selected-device-name", name);
    }
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

void Settings::on_show()
{
    if(visibleView.get_value() != "settings_view")
        return;

    Logger::debug("Showing settings dialog.");
    //Populate Combo Box:
    auto controllers = btHardwareInterface->get_controller_names();
    btHostCombo->remove_all();

    auto selectedController = settings->get_string("bluetooth-controller");

    Logger::debug("Selected bluetooth controller is " + selectedController);

    for(int i=0; i < controllers.size(); i++)
    {
        btHostCombo->append(controllers[i]);
        if(controllers[i] == selectedController) {
            Logger::debug("Setting active controller to " + selectedController);
            btHostCombo->set_active(i);
        }
    }
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
}
