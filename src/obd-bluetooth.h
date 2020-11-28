//This file defines the BlueTooth class that uses the old
//bluez-libs.  These libraries are deprecated in favor of
//of using d-bus to interface with bluetoothd instead.
//
//This version of the BlueTooth class was abandoned early
//in development and is not complete.  Use the implementation
//in obd-bluetooth-dbus.h instead.
//
//I will keep this version of the BlueTooth class around in
//case the d-bus implementation doesn't work out.

#ifndef _OBD_BLUETOOTH_
#define _OBD_BLUETOOTH_

#include <cstdint>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <vector>
#include <string>

class BlueTooth {
    public:
        BlueTooth();
        ~BlueTooth();

        //Host Controller Methods
        std::vector<int> get_controller_numbers();
        std::vector<std::string> get_controller_names();
        bool select_controller(std::string controller_name);
        bool select_controller(int controller_number);

        //Remote Device Methods
        std::vector<std::string> get_device_addresses();
        std::vector<std::string> get_device_names(int timeout=0);
        std::string get_device_address(unsigned int deviceIdx);
        std::string get_device_name(unsigned int deviceIdx, int timeout=0);
        int probe_remote_devices(unsigned int maxDevices = 255, 
                                 unsigned int probeTime = 10);
    private:
        std::vector<hci_dev_info> controllers;
        std::vector<inquiry_info> remoteDevices;
        hci_dev_info *selected_controller;
        int hci_sock;

        //Private Methods
        std::string get_device_name(inquiry_info &remoteDevice, int timeout);
        void select_controller(hci_dev_info &controller);

};

#endif
