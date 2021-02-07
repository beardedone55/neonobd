#include "settingsview.h"
#include "mainwindow.h"
#include <iostream>

SettingsView::SettingsView(MainWindow &window) :
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
    std::cout << "Creating SettingsView." << std::endl;
    set_name("settingsview");

    //Top row, return home button
    main_window.connect_view_button(homeButton, this, &main_window.homeView);
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
    bluetooth_rb.signal_toggled().connect(sigc::mem_fun(*this, &SettingsView::selectBluetooth));
    serial_rb.signal_toggled().connect(sigc::mem_fun(*this, &SettingsView::selectSerial));

    //Bluetooth specific options
    //---------------------------
    //Select host bluetooth device:
    btGrid.attach(btHostLabel,0,0);
    btGrid.attach(btHostCombo,1,0);

    btHostCombo.signal_changed().connect(sigc::mem_fun(*this, 
                                         &SettingsView::selectBluetoothController));

    //Select remote bluetooth device (OBD device):
    btGrid.attach(btDeviceLabel,0,1);
    btGrid.attach(btDeviceCombo,1,1);
    btGrid.attach(btDeviceScan,2,1);
    btDeviceScan.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::scanBluetooth));

    btDeviceCombo.signal_changed().connect(sigc::mem_fun(*this, 
                                           &SettingsView::selectBluetoothDevice));

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

SettingsView::~SettingsView() {}

void SettingsView::selectBluetooth()
{
    serialGrid.hide();
    btGrid.show();
    if(settings->get_enum("interface-type") != BLUETOOTH_IF)
        settings->set_enum("interface-type", BLUETOOTH_IF);
}

void SettingsView::selectSerial()
{
    btGrid.hide();
    serialGrid.show();
    if(settings->get_enum("interface-type") != SERIAL_IF)
        settings->set_enum("interface-type", SERIAL_IF);
}

void SettingsView::selectBluetoothController()
{
    auto bluetoothController = settings->get_string("bluetooth-controller");
    auto newBluetoothController = btHostCombo.get_active_text();
    if(newBluetoothController != bluetoothController)
    {
        settings->set_string("bluetooth-controller", newBluetoothController);
    }
    main_window.bluetooth.select_controller(newBluetoothController);
}

void SettingsView::selectBluetoothDevice()
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

void SettingsView::scanComplete()
{
    BlueTooth &bt = main_window.bluetooth;

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

void SettingsView::updateScanProgress(int percentComplete)
{
    if(percentComplete == 100)
        scanComplete();
    else
        btScanProgress.set_fraction(percentComplete / 100.0);
}

void SettingsView::scanBluetooth()
{
    BlueTooth &bt = main_window.bluetooth;
    btScanConnection = bt.signal_probe_progress().connect(
                       sigc::mem_fun(*this, &SettingsView::updateScanProgress));

    btScanProgress.set_fraction(0.0);
    btScanLabel.show();
    btScanProgress.show();
    
    btDeviceLabel.hide();
    btDeviceCombo.hide();
    btDeviceScan.hide();

    bt.probe_remote_devices();
}

void SettingsView::on_show()
{
    //Populate Combo Box:
    auto controllers = main_window.bluetooth.get_controller_names();
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
