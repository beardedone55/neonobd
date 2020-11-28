#include "settingsview.h"
#include "mainwindow.h"
#include <iostream>

SettingsView::SettingsView(MainWindow &window) :
   main_window{window},
   homeButton{"🏠⮨"},
   connectionFrame{"OBD Connection Settings"},
   bluetooth_rb{connectionTypes, "Bluetooth"},
   serial_rb{connectionTypes, "Serial Port/USB"},
   btHostLabel{"Bluetooth Host Controller"},
   btDeviceLabel{"Bluetooth OBD Device: "},
   btScanLabel{"Searching for Bluetooth Devices: "},
   btDeviceScan{"🔍"}
{
    std::cout << "Creating SettingsView." << std::endl;
    set_name("settingsview");
    
    //Top row, return home button
    main_window.connect_view_button(homeButton, this, &main_window.homeView);
    pack_start(topRow, Gtk::PACK_SHRINK);
    topRow.add(homeButton);
    topRow.set_layout(Gtk::BUTTONBOX_START);

    //Connection Settings
    pack_start(connectionFrame);
    connectionFrame.add(connectionGrid);
    //Radio Buttons to select Bluetooth vs. serial
    connectionGrid.add(bluetooth_rb);
    connectionGrid.add(serial_rb);
    bluetooth_rb.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::selectBluetooth));
    serial_rb.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::selectSerial));

    //Bluetooth specific options
    //---------------------------
    //Select host bluetooth device:
    btGrid.add(btHostLabel);
    btGrid.add(btHostCombo);

    //Select remote bluetooth device (OBD device):
    btGrid.attach_next_to(btDeviceLabel, Gtk::POS_BOTTOM);
    btGrid.attach_next_to(btDeviceCombo, btDeviceLabel, Gtk::POS_RIGHT);
    btGrid.attach_next_to(btDeviceScan, btDeviceCombo, Gtk::POS_RIGHT);
    btDeviceScan.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::scanBluetooth));

    btGrid.attach_next_to(btScanLabel, btDeviceLabel, Gtk::POS_BOTTOM);
    btGrid.attach_next_to(btScanProgress, btScanLabel, Gtk::POS_RIGHT);
    btScanProgress.set_valign(Gtk::ALIGN_CENTER);

    connectionGrid.attach_next_to(btGrid, Gtk::POS_BOTTOM, 2);

    show_all_children();

    btScanLabel.hide();
    btScanProgress.hide();

}

SettingsView::~SettingsView() {}

void SettingsView::selectBluetooth()
{
    serialGrid.hide();
    btGrid.show();
}

void SettingsView::selectSerial()
{
    btGrid.hide();
    serialGrid.show();
}

void SettingsView::scanComplete()
{
    BlueTooth &bt = main_window.bluetooth;

    //Update Combo Box
    auto devices = bt.get_device_names_addresses();
    btDeviceCombo.remove_all();
    for (auto &[name, address] : devices)
        btDeviceCombo.append(address + "<" + name + ">");

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
    for(auto &device : controllers)
        btHostCombo.append(device);

    //If we only have one controller, asking user to select one is silly.
    if(controllers.size() == 1)
    {
       main_window.bluetooth.select_controller(controllers[0]);
       btHostLabel.hide();
       btHostCombo.hide();
    }

    //No bluetooth available...
    if(controllers.size() == 0)
    {
        serial_rb.clicked();
        bluetooth_rb.set_sensitive(false); //Disable Bluetooth
    }
    else
        bluetooth_rb.set_sensitive(true); //Enalbe Bluetooth

    Gtk::VBox::on_show();
}
