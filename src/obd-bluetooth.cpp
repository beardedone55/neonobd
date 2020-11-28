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

#include "obd-bluetooth.h"
#include <unistd.h>
#include <iostream>

BlueTooth::BlueTooth() :
    hci_sock{-1},
    selected_controller{NULL}

{
    //Find all the bluetooth controllers,
    //and add the device info to the list
    for(int i=0; i < HCI_MAX_DEV; i++)
    {
        hci_dev_info dev_info;
        if (hci_devinfo(i, &dev_info) == 0)
            controllers.push_back(dev_info);
    }
}

BlueTooth::~BlueTooth()
{
    if(hci_sock >= 0)
        close(hci_sock);
}

void BlueTooth::select_controller(hci_dev_info &controller)
{
    selected_controller = &controller;

    if(hci_sock >= 0)
        close(hci_sock);

    hci_sock = hci_open_dev(controller.dev_id);
}

bool BlueTooth::select_controller(std::string controller_name)
{
    for(auto& it : controllers)
    {
        std::string name {it.name};
        if(controller_name.compare(name) == 0)
        {
            select_controller(it);
            return true;
        }
    }
    return false;
}

bool BlueTooth::select_controller(int controller_number)
{
    for(auto& it : controllers)
    {
        if(controller_number == it.dev_id)
        {
            select_controller(it);
            return true;
        }
    }
    return false;
}

std::vector<int> BlueTooth::get_controller_numbers()
{
    std::vector<int> ret;

    ret.reserve(controllers.size());
    for(auto& it : controllers)
        ret.push_back(it.dev_id);

    return ret;
}

std::vector<std::string> BlueTooth::get_controller_names()
{
    std::vector<std::string> ret;

    ret.reserve(controllers.size());
    for(auto& it : controllers)
        ret.emplace_back(it.name);

    return ret;
}

int BlueTooth::probe_remote_devices(unsigned int maxDevices, unsigned int probeTime)
{
    remoteDevices.clear();

    if(!selected_controller)
        return 0;

    int dev_id = selected_controller->dev_id;

    inquiry_info *inquiryInfo = new inquiry_info[maxDevices];

    int ret = hci_inquiry(dev_id, probeTime, maxDevices, 
                           NULL, &inquiryInfo, IREQ_CACHE_FLUSH);

    if(ret < 0)
        ret = 0;

    remoteDevices.reserve(ret);

    for(int i=0; i < ret; i++)
        remoteDevices.push_back(inquiryInfo[i]);    

    delete[] inquiryInfo;

    return ret;
}

std::string BlueTooth::get_device_name(inquiry_info &remoteDevice, int timeout)
{
    char name[248];
    if(hci_read_remote_name(hci_sock, &remoteDevice.bdaddr, 
                            sizeof(name), name, timeout) == 0)
        return name;
    
    return "";
}

std::string BlueTooth::get_device_name(unsigned int deviceIdx, int timeout)
{
    if(remoteDevices.size() > deviceIdx)
        return get_device_name(remoteDevices[deviceIdx], timeout);

    return "";
}

std::string BlueTooth::get_device_address(unsigned int deviceIdx)
{
    if(remoteDevices.size() > deviceIdx)
    {
        char addr[18];
        if(ba2str(&remoteDevices[deviceIdx].bdaddr, addr) == 0)
            return addr;
    }
    return "";
}

std::vector<std::string> BlueTooth::get_device_addresses()
{
    std::vector<std::string> ret;

    ret.reserve(remoteDevices.size());

    for (auto& it : remoteDevices)
    {
        char addr[18];
        ba2str(&it.bdaddr, addr);
        ret.emplace_back(addr);
    }

    return ret;
}

std::vector<std::string> BlueTooth::get_device_names(int timeout)
{
    std::vector<std::string> ret;
    int i = 0;

    ret.reserve(remoteDevices.size());

    for (auto& it : remoteDevices)
        ret.push_back(get_device_name(it, timeout));

    return ret;
}
