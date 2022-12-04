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

Settings::Settings(MainWindow &window) :
   Box{Gtk::Orientation::VERTICAL, 0},
   main_window{window},
   homeButton{"🏠⮨"},
   connectionFrame{"OBD Connection Settings"},
   bluetooth_rb{"Bluetooth"},
   serial_rb{"Serial Port/USB"},
   btHostLabel{"Bluetooth Host Controller"},
   btDeviceLabel{"Bluetooth OBD Device: "},
   btScanLabel{"Searching for Bluetooth Devices: "},
   btDeviceScan{"🔍"}
{
    set_name("settingsview");

    //Top row, return home button
    main_window.connect_view_button(homeButton, this, &main_window.home);
    append(topRow);
    topRow.append(homeButton);
    //topRow.set_layout(Gtk::BUTTONBOX_START);

    //Connection Settings
    append(connectionFrame);
    connectionFrame.set_child(connectionGrid);
    //Radio Buttons to select Bluetooth vs. serial
    serial_rb.set_group(bluetooth_rb);  //link radio buttons together
    connectionGrid.attach(bluetooth_rb,0,0);
    connectionGrid.attach(serial_rb,1,0);
    bluetooth_rb.signal_toggled().connect(sigc::mem_fun(*this, &Settings::selectBluetooth));
    serial_rb.signal_toggled().connect(sigc::mem_fun(*this, &Settings::selectSerial));

    //Bluetooth specific options
    //---------------------------
    //Select host bluetooth device:
    btGrid.attach(btHostLabel,0,0);
    btGrid.attach(btHostCombo,1,0);

    btHostCombo.signal_changed().connect(sigc::mem_fun(*this, 
                                         &Settings::selectBluetoothController));

    //Select remote bluetooth device (OBD device):
    btGrid.attach(btDeviceLabel,0,1);
    btGrid.attach(btDeviceCombo,1,1);
    btGrid.attach(btDeviceScan,2,1);
    btDeviceScan.signal_clicked().connect(sigc::mem_fun(*this, &Settings::scanBluetooth));

    btDeviceCombo.signal_changed().connect(sigc::mem_fun(*this, 
                                           &Settings::selectBluetoothDevice));

    btGrid.attach(btScanLabel,0,3);
    btGrid.attach(btScanProgress,1,3);
    btScanProgress.set_valign(Gtk::Align::CENTER);

    connectionGrid.attach(btGrid,0,4,2);

    //show_all_children();

    btScanLabel.hide();
    btScanProgress.hide();

    //Load Settings
    settings = Gio::Settings::create("com.github.beardedone55.neonobd");
    if(settings->get_enum("interface-type") == BLUETOOTH_IF)
        bluetooth_rb.set_active();
    else
        serial_rb.set_active();

    auto controllerName = settings->get_string("bluetooth-controller");
    if(controllerName != "")
    {
        btHostCombo.append(controllerName);
        btHostCombo.set_active(0);
    }

    auto deviceAddress = settings->get_string("selected-device-address");
    if(deviceAddress != "")
    {
        auto deviceName = settings->get_string("selected-device-name");
        btDeviceCombo.append(deviceAddress, deviceName);
        btDeviceCombo.set_active(0);
    }
}

Settings::~Settings() {}

void Settings::selectBluetooth()
{
    serialGrid.hide();
    btGrid.show();
    if(settings->get_enum("interface-type") != BLUETOOTH_IF)
        settings->set_enum("interface-type", BLUETOOTH_IF);
}

void Settings::selectSerial()
{
    btGrid.hide();
    serialGrid.show();
    if(settings->get_enum("interface-type") != SERIAL_IF)
        settings->set_enum("interface-type", SERIAL_IF);
}

void Settings::selectBluetoothController()
{
    auto bluetoothController = settings->get_string("bluetooth-controller");
    auto newBluetoothController = btHostCombo.get_active_text();
    if(newBluetoothController != bluetoothController)
    {
        settings->set_string("bluetooth-controller", newBluetoothController);
    }
    main_window.bluetoothSerialPort.select_controller(newBluetoothController);
}

void Settings::selectBluetoothDevice()
{
    auto defaultAddress = settings->get_string("selected-device-address");
    auto activeDevice = btDeviceCombo.get_active_values();
    if(activeDevice)
    {
        auto [ address, name ] = *activeDevice;
        settings->set_string("selected-device-address", address);
        settings->set_string("selected-device-name", name);
    }
}

void Settings::scanComplete()
{
    BluetoothSerialPort &bt = main_window.bluetoothSerialPort;

    //Update Combo Box
    auto devices = bt.get_device_names_addresses();
    btDeviceCombo.remove_all();

    auto defaultAddress = settings->get_string("selected-device-address");
    auto defaultName = settings->get_string("selected-device-name");
    if (defaultAddress != "")
        btDeviceCombo.append(defaultAddress, defaultName);

    for (auto &[address, name] : devices)
        if(address != defaultAddress)
            btDeviceCombo.append(address, "<" + name + ">");

    btDeviceCombo.set_active(0);

    btScanConnection.disconnect();
    
    //Hide Progress Bar
    btScanProgress.hide();
    btScanLabel.hide();

    //Show Device List
    btDeviceLabel.show();
    btDeviceCombo.show();
    btDeviceScan.show();
}

void Settings::updateScanProgress(int percentComplete)
{
    if(percentComplete == 100)
        scanComplete();
    else
        btScanProgress.set_fraction(percentComplete / 100.0);
}

void Settings::scanBluetooth()
{
    BluetoothSerialPort &bt = main_window.bluetoothSerialPort;
    btScanConnection = bt.signal_probe_progress().connect(
                       sigc::mem_fun(*this, &Settings::updateScanProgress));

    btScanProgress.set_fraction(0.0);
    btScanLabel.show();
    btScanProgress.show();
    
    btDeviceLabel.hide();
    btDeviceCombo.hide();
    btDeviceScan.hide();

    bt.probe_remote_devices();
}

void Settings::on_show()
{
    //Populate Combo Box:
    auto controllers = main_window.bluetoothSerialPort.get_controller_names();
    btHostCombo.remove_all();

    auto selectedController = settings->get_string("bluetooth-controller");
    for(int i=0; i < controllers.size(); i++)
    {
        btHostCombo.append(controllers[i]);
        if(controllers[i] == selectedController)
            btHostCombo.set_active(i);
    }
    //If we only have one controller, asking user to select one is silly.
    if(controllers.size() == 1)
    {
       btHostCombo.set_active(0);
       btHostLabel.hide();
       btHostCombo.hide();
    }
    else
    {
       btHostLabel.show();
       btHostCombo.show();
    }

    //No bluetooth available...
    if(controllers.size() == 0)
    {
        serial_rb.set_active();
        bluetooth_rb.set_sensitive(false); //Disable Bluetooth
    }
    else
        bluetooth_rb.set_sensitive(true); //Enalbe Bluetooth

    Gtk::Box::on_show();
}
