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

#include "bluetooth-serial-port.hpp"
#include "dbus-type.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

// time.h provides timeval
#include <sys/time.h> //NOLINT(misc-include-cleaner)
#include <systemd/sd-bus.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

using BTSP = BluetoothSerialPort;

namespace {
    constexpr int FINISHED = 100;
} // namespace

BTSP::BluetoothSerialPort() : 
    m_system_bus{get_system_dbus()},
    m_event{get_dbus_event()}
{
    get_objects();
    Logger::debug("Created BluetoothSerialPort.");
}

BTSP::~BluetoothSerialPort() {
    if(!m_connected_device_path.empty()) {
        disconnect(nullptr);
    }

    const std::unique_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= -1) {
        close(m_sock_fd);
    }


    Logger::debug << "Destroyed BluetoothSerialPort.\n";
}

BTSP::DBusPtr BTSP::get_system_dbus() {
    sd_bus* system_bus = nullptr;
    if(sd_bus_open_system(&system_bus) < 0) {
        Logger::error("Error connecting to system DBUS.");
    }
    return DBusPtr(system_bus, [](sd_bus* b){sd_bus_flush_close_unref(b);});
}

BTSP::DBusEventPtr BTSP::get_dbus_event() {
    sd_event* event = nullptr;
    if(!m_system_bus || sd_event_default(&event) < 0) {
        Logger::error("Error getting sd_bus event loop.");
    } else if(sd_bus_attach_event(m_system_bus.get(), event, SD_EVENT_PRIORITY_NORMAL) < 0) {
        Logger::error("Error attaching event loop to DBUS.");
        event = sd_event_unref(event);
    }
    return DBusEventPtr(event, [](sd_event* e){sd_event_unref(e);});
}

bool BTSP::connect_object_manager(sd_bus_message_handler_t callback,
                                                 std::string_view method_name) {

    if(sd_bus_match_signal_async(m_system_bus.get(), nullptr, "org.bluez", "/",
                                 "org.freedesktop.DBus.ObjectManager", method_name.data(),
                                 callback, nullptr, this) < 0) {
        Logger::error << "Error connecting to " << method_name << "\n";
        return false;
    }
    return true;
}

void BTSP::connect_object_manager() {
    if(!m_system_bus || 
       !connect_object_manager(add_object, "InterfacesAdded") ||
       !connect_object_manager(remove_object, "InterfacesRemoved")) {

        Logger::error("Error connecting to object manager.");

    }
}

int BTSP::finish_connection(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    Logger::debug("Connection finished.");
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);

    if(!bt_ptr->m_complete_connection) {
        Logger::error << "Connection completion handler not registered...\n";
        return 0;
    }

    if(sd_bus_message_is_method_error(reply, nullptr)) { 
        Logger::error("Error occurred connecting to Bluetooth Device");
        bt_ptr->m_complete_connection(false);
    } else {
        bt_ptr->m_complete_connection(true);
    }
    bt_ptr->m_complete_connection = nullptr;
    return 0;
}

int BTSP::call_dbus(std::string_view path, std::string_view interface, std::string_view method, sd_bus_message_handler_t callback) {

    int e = 0;
    if(callback) {
        Logger::debug << "Calling async method " << method << "\n";
        e = sd_bus_call_method_async(m_system_bus.get(), nullptr, "org.bluez", path.data(),
                                     interface.data(), method.data(), callback, this, nullptr);
    } else {
        Logger::debug << "Calling sync method " << method << "\n";
        e = sd_bus_call_method(m_system_bus.get(), "org.bluez", path.data(), interface.data(),
                               method.data(), nullptr, nullptr, nullptr);
    }

    if(e < 0) {
        Logger::error << "Call to " << method << " failed: " << std::strerror(-e) << "(" << -e << ")\n";
    }

    return e;
}

void BTSP::initiate_connection(const std::string& device_address) {
    Logger::debug("Initiating Bluetooth connection.");
    Logger::debug << "Device " << device_address << " : " << m_dev_name_path_map[device_address] << "\n";
    call_dbus(m_dev_name_path_map[device_address], "org.bluez.Device1", "Connect", finish_connection);
}

int BTSP::finish_disconnect(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    Logger::debug("Disconnect finished.");
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    if(bt_ptr->m_complete_disconnect) {
        bt_ptr->m_complete_disconnect();
    }
    bt_ptr->m_complete_disconnect = nullptr;
    return 0;
}

void BTSP::initiate_disconnect(const std::string& device_path) {
    Logger::debug("Initiating Bluetooth disconnect.");

    sd_bus_message_handler_t callback = (m_complete_disconnect) ? finish_disconnect : nullptr;
    call_dbus(device_path, "org.bluez.Device1", "Disconnect", callback);
}

void BTSP::disconnect(std::function<void()> callback) {
    if(m_complete_disconnect) {
        Logger::error << "Disconnect called but already in progress.";
        return;
    }
    if(m_connected_device_path.empty()) {
        Logger::error << "Disconnect called but no device connected.";
        return;
    }
    m_complete_disconnect = callback;
    initiate_disconnect(m_connected_device_path);
}

void BTSP::pre_connection_scan_progress(int percent_complete, const std::string& device_address) {

    if (m_dev_name_path_map.contains(device_address)) {
        Logger::debug("Device " + device_address + " found.");
        initiate_connection(device_address);
        m_probe_callback = nullptr;
    } else if (percent_complete == FINISHED) {
        // Could not find device
        m_probe_callback = nullptr;
        if(m_complete_connection) {
            m_complete_connection(false);
            m_complete_connection = nullptr;
        }
    }

    // Device not found... just keep probing....
}

bool BTSP::connect(const std::string& device_address, std::function<void(bool)> callback) {
    if(m_complete_connection) {
        Logger::error << "Connection already in progress...\n";
        return false;
    }

    m_complete_connection = callback;

    if (!m_dev_name_path_map.contains(device_address)) {
        // Device not in inventory.  Perform device discovery before attemting
        // to connect.
        Logger::debug("Device " + device_address + " not found in inventory.");
        probe_remote_devices(
            [this, device_address](int percent_complete) {
                pre_connection_scan_progress(percent_complete, device_address);
            });
                
        return true;
    }

    initiate_connection(device_address);
    return true;
}

namespace {
// timeval is provided by sys/time.h
// NOLINTNEXTLINE(misc-include-cleaner)
timeval milliseconds_to_time_val(std::chrono::milliseconds time) {
    const std::chrono::seconds seconds =
        std::chrono::duration_cast<std::chrono::seconds>(time);
    time -= seconds;
    return {seconds.count(), std::chrono::microseconds(time).count()};
}
} // namespace

void BTSP::set_timeout(std::chrono::milliseconds timeout) {
    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= 0) {
        // timeval is provided by sys/time.h
        // NOLINTNEXTLINE(misc-include-cleaner)
        const timeval time = milliseconds_to_time_val(timeout);
        // SOL_SOCKET, SO_RCVTIMEO, and SO_SNDTIMEO are provided
        // by sys/socket.h
        // NOLINTBEGIN(misc-include-cleaner)
        setsockopt(m_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
        setsockopt(m_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time));
        // NOLINTEND(misc-include-cleaner)
    }
}

void BTSP::get_objects() {
    call_dbus("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", get_objects_complete);
}

int BTSP::get_objects_complete(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    if(sd_bus_message_is_method_error(reply, nullptr)) {
        Logger::error << "Failed to get DBus Objects from Object Manager.\n";
        return 0;
    }
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    DBusType msg(*reply);
    bt_ptr->connect_object_manager();
    for(auto& [path, obj] : msg.getDict()) {
        bt_ptr->add_object(std::get<std::string>(path), obj);
    }
    bt_ptr->register_profile();
    bt_ptr->register_agent();
    return 0;
}

int BTSP::add_object(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    std::string path = DBusType(*msg).getString();
    DBusType obj(*msg);
    return bt_ptr->add_object(path, obj);
}

int BTSP::remove_object(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    std::string path = DBusType(*msg).getString();
    DBusType obj(*msg);
    return bt_ptr->remove_object(path, obj);
}

int BTSP::add_object(const std::string& path, const DBusType& obj) {
    Logger::debug << "Added " << path << ".\n";
    for(const auto& [ interface, properties ] : obj.getDict()) {

        const auto interface_name = std::get<std::string>(interface);

        if(interface_name == "org.bluez.Adapter1") {
            if(m_controllers.contains(path)) {
                m_controllers.at(path) = std::move(properties);
            } else {
                m_controllers.emplace(path, std::move(properties));
            }
        } else if (interface_name == "org.bluez.Device1") {
            if(m_remote_devices.contains(path)) {
                m_remote_devices.at(path) = std::move(properties);
            } else {
                m_remote_devices.emplace(path, std::move(properties));
            }
            m_dev_name_path_map[properties["Address"].getString()] = path;
            Logger::debug << "Added Device " << properties["Alias"].getString() << " : " 
                << properties["Address"].getString() << "\n";
        } else if (interface_name == "org.bluez.AgentManager1") {
            m_agent_manager = path;
        } else if (interface_name == "org.bluez.ProfileManager1") {
            m_profile_manager = path;
        }
    }
    return 0;
}

int BTSP::remove_object(const std::string& path, const DBusType& obj) {
    Logger::debug << "Removed " << path << ".\n";
    for(const auto& interface : obj.getArray()) {

        auto interface_name = interface.getString();

        if(interface_name == "org.bluez.Adapter1") {
            m_controllers.erase(path);
        } else if (interface_name == "org.bluez.Device1") {
            if(m_remote_devices.contains(path)) {
                m_dev_name_path_map.erase(m_remote_devices.at(path)["Address"].getString());
                m_remote_devices.erase(path);
            }
        } else if (interface_name == "org.bluez.AgentManager1") {
            m_agent_manager = "";
        } else if (interface_name == "org.bluez.ProfileManager1") {
            m_profile_manager = "";
        }
    }
    return 0;
}

void BTSP::process_events(std::chrono::microseconds timeout) {
    if(m_event) {
        while(sd_event_run(m_event.get(), timeout.count()) > 0); 
    }
}

bool BTSP::select_controller(
    const std::string& controller_name) {
    if (m_controllers.contains(controller_name)) {
        m_selected_controller = controller_name;
        return true;
    }

    return false;
}

std::vector<std::string> BTSP::get_controller_names() {
    std::vector<std::string> ret;

    ret.reserve(m_controllers.size());
    std::transform(m_controllers.begin(), m_controllers.end(),
                   std::back_inserter(ret),
                   [](const auto& ctlr) { return ctlr.first; });
    return ret;
}

void BTSP::emit_probe_progress(int percent_complete) {
    if (percent_complete == FINISHED) {
        m_probe_in_progress = false;
    }
    if(m_probe_callback) {
        m_probe_callback(percent_complete);
    }
}

int BTSP::stop_probe_finish(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    if(sd_bus_message_is_method_error(reply, nullptr)) {
        Logger::error << "StopDiscovery completed in error\n";
    }

    bt_ptr->emit_probe_progress(FINISHED);
    return 0;
}

void BTSP::stop_probe() {
    // Timeout occurred; stop probing devices.
    if (!m_selected_controller.empty()) {
        auto ctlr = m_selected_controller;
        int e = call_dbus(ctlr, "org.bluez.Adapter1", "StopDiscovery", stop_probe_finish);
        
        if(e < 0) {
            emit_probe_progress(FINISHED); // Cannot probe devices
        }
    } else {
        emit_probe_progress(FINISHED);
    }
}

std::uint64_t BTSP::get_tick_time(std::chrono::seconds probe_time) {
    static constexpr int TICK_COUNT = 100;
    return std::chrono::microseconds(probe_time).count() / TICK_COUNT;
}

int BTSP::update_probe_progress(sd_event_source *s, std::uint64_t usec, void* userdata) {
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    bt_ptr->emit_probe_progress(bt_ptr->m_probe_progress++);
    if (bt_ptr->m_probe_progress == FINISHED) {
        sd_event_source_set_enabled(s, SD_EVENT_OFF);
        sd_event_source_unref(s);
        bt_ptr->stop_probe();
    } else {
        sd_event_source_set_time_relative(s, get_tick_time(bt_ptr->m_probe_time));
    }
    return 0;
}

int BTSP::probe_finish(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    // StartDiscovery command issued; now wait for timeout

    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);

    if(sd_bus_message_is_method_error(reply, nullptr)) { 
        Logger::error << "DeviceDiscovery completed in error\n";
        bt_ptr->emit_probe_progress(FINISHED);
        return 0;
    }

    // Convert timeout to milliseconds, and interrupt every time
    // we are 100th the way to completion

    bt_ptr->m_probe_progress = 0;

    sd_event_source* s = nullptr;
    sd_event_add_time_relative(sd_bus_get_event(bt_ptr->m_system_bus.get()), &s, CLOCK_MONOTONIC, 
                               get_tick_time(bt_ptr->m_probe_time), 0, update_probe_progress, userdata);
    sd_event_source_set_enabled(s, SD_EVENT_ON);
    return 0;
}

void BTSP::probe_remote_devices(std::function<void(int)> callback, std::chrono::seconds time) {
    Logger::debug("Probing remote Bluetooth devices.");
    if (m_probe_in_progress) {
        return;
    }

    if (!m_selected_controller.empty()) {
        m_probe_in_progress = true;
        m_probe_time = time;
        m_probe_callback = callback;
        auto ctlr = m_selected_controller;
        int e = call_dbus(ctlr, "org.bluez.Adapter1", "StartDiscovery", probe_finish);

        if(e < 0) {
            emit_probe_progress(FINISHED); // Cannot probe devices
        }
    } else {
        Logger::error("No bluetooth controller selected.");
        emit_probe_progress(FINISHED); // Cannot probe devices
    }
}

std::vector<BTSP::DeviceInfo> BTSP::get_device_names_addresses() {
    std::vector<DeviceInfo> ret;

    for(const auto& [path, properties] : m_remote_devices) {
        ret.emplace_back(properties["Alias"].getString(), properties["Address"].getString());
    }

    return ret;
}

constexpr auto OBJECT_PATH = "/com/github/beardedone55/bluetooth_serial";

// Bluetooth profile UUID for Serial Port Profile (SPP)
// See
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/
constexpr auto SERIAL_PORT_UUID = "00001101-0000-1000-8000-00805f9b34fb";

int BTSP::register_complete(sd_bus_message* msg, void* userdata, sd_bus_error* error) {
    if(sd_bus_message_is_method_error(msg, nullptr)) {
        Logger::error << "Error occurred registering agent.\n";
    }
    Logger::debug("Bluetooth agent registration complete.");
    return 0;
}

int BTSP::register_object(std::string_view interface, const sd_bus_vtable vtable[]) {
    if (m_system_bus) {
        Logger::debug << "Registering object " << interface << "\n";

        return sd_bus_add_object_vtable(m_system_bus.get(), nullptr, OBJECT_PATH,
                                        interface.data(), vtable, this);

    }

    return 0;
}

int BTSP::bt_release(sd_bus_message*, void*, sd_bus_error*) {
    // Profile was removed by profile manager.
    // Some cleanup could be done, but for now, just ignore.
    // No response is expected.
    return 0;
}

int BTSP::bt_new_connection(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    std::string obj_path = DBusType(*msg).getString();
    std::int32_t fd = DBusType(*msg).getValue<std::int32_t>();
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);

    Logger::debug("New Bluetooth connection requested.");
    if (bt_ptr->m_sock_fd < 0) {
        bt_ptr->m_connected_device_path = obj_path;
        // Grab the socket, so we can communicate with
        // device, and return to acknowlege connection.
        {
            const std::unique_lock lock(bt_ptr->m_sock_fd_mutex);
            bt_ptr->m_sock_fd = dup(fd);
        }
        Logger::debug("File descriptor for Bluetooth device: " +
                      std::to_string(bt_ptr->m_sock_fd));
        // Returns: void
        sd_bus_reply_method_return(msg, "");
    } else { // We are already connected to a device (Shouldn't happen...) 
        // Close the new socket and return an error.
        Logger::error << "File descriptor was already set to "
                      << bt_ptr->m_sock_fd;
        sd_bus_reply_method_errorf(msg, "org.bluez.Error.Rejected",
                                   "Already connected to a bluetooth device.");
    }

    return 0;
}

int BTSP::bt_request_disconnection(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    std::string obj_path = DBusType(*msg).getString();
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    if (bt_ptr->m_sock_fd >= 0 &&
        obj_path == bt_ptr->m_connected_device_path) {
        {
            const std::unique_lock lock(bt_ptr->m_sock_fd_mutex);
            close(bt_ptr->m_sock_fd);
            bt_ptr->m_sock_fd = -1;
        }
        // Returns void
        sd_bus_reply_method_return(msg, "");
    } else { // Disconnect requested for unknown device
        // Return an error
        sd_bus_reply_method_errorf(msg, "org.bluez.Error.Rejected",
                                   "Unexpected device requested disconnection: %s.", obj_path.c_str());
    }
    bt_ptr->m_connected_device_path = "";
    return 0;
}

const sd_bus_vtable BTSP::profile_vtable[] {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS(
        "Release",
        SD_BUS_NO_ARGS,
        SD_BUS_NO_RESULT,
        BTSP::bt_release,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "NewConnection",
        SD_BUS_ARGS(
            "o", device,
            "h", fd, 
            "a{sv}", fd_properties
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_new_connection,
        0   
    ),  
    SD_BUS_METHOD_WITH_ARGS(
        "RequestDisconnection",
        SD_BUS_ARGS(
            "o", device
        ),  
        SD_BUS_NO_RESULT,
        BTSP::bt_request_disconnection,
        0   
    ),  
    SD_BUS_VTABLE_END
};

void BTSP::register_profile() {
    if (m_profile_manager.empty()) {
        return;
    }

    if(register_object("org.bluez.Profile1", profile_vtable) < 0) {
        Logger::error << "Error occurred registering profile with DBus\n";
        return;
    }

    sd_bus_call_method_async(m_system_bus.get(), nullptr, "org.bluez", m_profile_manager.c_str(),
                             "org.bluez.ProfileManager1", "RegisterProfile", register_complete,
                             nullptr, "osa{sv}", OBJECT_PATH, SERIAL_PORT_UUID, 5,
                             "Name", "s", "obd-serial", "Service", "s", SERIAL_PORT_UUID,
                             "Role", "s", "client", "Channel", "q", static_cast<std::uint16_t>(1),
                             "AutoConnect", "b", 1);


    Logger::debug("Registering bluetooth profile manager.");
}

int BTSP::bt_agent_request(sd_bus_message* msg, void* userdata, const std::string& text, 
                            const ResponseType response_type) {
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    auto device_path = DBusType(*msg).getString();
    std::string device_name;
    if(bt_ptr->m_remote_devices.contains(device_path)) {
        device_name = bt_ptr->m_remote_devices.at(device_path)["Alias"].getString();
    } else { 
        device_name = device_path;
    }
    std::string punctuation;
    if(response_type == neon::USER_YN) {
        punctuation = "?";
    } else {
        punctuation = ".";
    }
    bt_ptr->request_from_user(text + device_name + punctuation,
                                     response_type, msg);
    return 1;
}

std::string BTSP::format_passkey(std::uint32_t value) {
    std::stringstream ss;
    static constexpr int PASSKEY_SIZE = 6;
    ss << std::setfill('0') << std::setw(PASSKEY_SIZE) << value;
    return ss.str();
}

template<typename T>
int BTSP::bt_agent_display(sd_bus_message* msg, void* userdata, const std::string& text) {
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    std::string device_path = DBusType(*msg).getString();
    std::string value_str;
    if constexpr (std::is_integral_v<T>) {
        T value = DBusType(*msg).getValue<T>();
        value_str = format_passkey(value);
    } else {
        value_str = DBusType(*msg).getString();
    }
    bt_ptr->request_from_user(text + device_path + " is " + value_str + ".",
                                     neon::USER_NONE, nullptr);
    sd_bus_reply_method_return(msg, "");
    return 0;
}

int BTSP::bt_agent_release(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return 0;
}

int BTSP::bt_request_pin_code(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return bt_agent_request(msg, userdata, "Enter Pin Code for ");
}

int BTSP::bt_display_pin_code(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return bt_agent_display<std::string>(msg, userdata, "Pin Code for ");
}

int BTSP::bt_request_passkey(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return bt_agent_request(msg, userdata, "Enter Passkey (0-9999999) for ", neon::USER_INT);
}

int BTSP::bt_display_passkey(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return bt_agent_display<std::uint32_t>(msg, userdata, "Passkey for ");
}

int BTSP::bt_request_confirmation(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    BluetoothSerialPort* bt_ptr = static_cast<BTSP*>(userdata);
    std::string device_path = DBusType(*msg).getString();
    std::uint32_t passkey = DBusType(*msg).getValue<std::uint32_t>();
    std::string passkey_str = format_passkey(passkey);
    bt_ptr->request_from_user(
        "Confirm Passkey for " + device_path + " is " + passkey_str + ".",
        neon::USER_YN, msg);

    return 1;
}

int BTSP::bt_request_authorization(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    return bt_agent_request(msg, userdata, "Authorize Connection to Device ", neon::USER_YN);
}

int BTSP::bt_authorize_service(sd_bus_message* msg, void*, sd_bus_error*) {
    // I don't know what this is supposed to do, so we'll just
    // return an error for now.
    sd_bus_reply_method_errorf(msg, "org.bluez.Error.Rejected",
                               "Unknown Method AuthorizeService");
    return 0;
}

int BTSP::bt_agent_cancel(sd_bus_message* msg, void*, sd_bus_error*) {
    sd_bus_reply_method_return(msg, "");
    return 0;
}

const sd_bus_vtable BTSP::agent_vtable[] {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS( 
        "Release",
        SD_BUS_NO_ARGS,
        SD_BUS_NO_RESULT,        
        BTSP::bt_agent_release,
        0                        
    ),                           
    SD_BUS_METHOD_WITH_ARGS(
        "RequestPinCode",    
        SD_BUS_ARGS( 
            "o", device
        ),
        SD_BUS_RESULT(
            "s", pincode
        ),
        BTSP::bt_request_pin_code,
        0               
    ),  
    SD_BUS_METHOD_WITH_ARGS(
        "DisplayPinCode",
        SD_BUS_ARGS(
            "o", device,
            "s", pincode
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_display_pin_code,
        0                  
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "RequestPasskey",
        SD_BUS_ARGS(
            "o", device
        ),
        SD_BUS_RESULT(
            "u", passkey
        ),
        BTSP::bt_request_passkey,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "DisplayPasskey",
        SD_BUS_ARGS(
            "o", device,
            "u", passkey,
            "q", entered
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_display_passkey,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "RequestConfirmation",
        SD_BUS_ARGS(
            "o", device,
            "u", passkey
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_request_confirmation,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "RequestAuthorization",
        SD_BUS_ARGS(
            "o", device
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_request_authorization,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "AuthorizeService",
        SD_BUS_ARGS(
            "o", device,
            "s", uuid
        ),
        SD_BUS_NO_RESULT,
        BTSP::bt_authorize_service,
        0
    ),
    SD_BUS_METHOD_WITH_ARGS(
        "Cancel",
        SD_BUS_NO_ARGS,
        SD_BUS_NO_RESULT,
        BTSP::bt_agent_cancel,
        0
    ),
    SD_BUS_VTABLE_END
};

void BTSP::register_agent() {
    if (m_agent_manager.empty()) {
        return;
    }

    if(register_object("org.bluez.Agent1", agent_vtable) < 0) {
        Logger::error << "Error occurred registering agent with DBus.\n";
        return;
    }

	sd_bus_call_method_async(m_system_bus.get(), nullptr, "org.bluez", m_agent_manager.c_str(),
                             "org.bluez.AgentManager1", "RegisterAgent", register_complete,
							 nullptr, "os", OBJECT_PATH, "KeyboardDisplay");

    Logger::debug("Registering bluetooth agent manager.");
}

void BTSP::request_from_user(
    const std::string& message, const ResponseType response_type,
    sd_bus_message* msg) {
    // This message emits a signal that should solicit a response from the
    // user. It does the work for "RequestPinCode", "RequestPasskey",
    //"RequestAuthorization", and "RequestConfirmation".

    if (m_request_user_input) {
        if(msg != nullptr) {
            sd_bus_message_ref(msg);
        }
        m_request_user_input(message, response_type, msg);
        // User is responsible for invoking respond_from_user()
        // to complete operation.
    } else if (msg) {
        // No one has registered with the signal.
        // Return an error.
        sd_bus_reply_method_errorf(msg, "org.bluez.Error.Rejected", "No handler for method.");
    }
}

void BTSP::respond_from_user(const ResponseVariant& response, void* handle) {
    sd_bus_message* msg = static_cast<sd_bus_message*>(handle);
    if (msg) {
        if (std::holds_alternative<bool>(response)) {
            const bool bool_response = std::get<bool>(response);
            if (bool_response) {
                sd_bus_reply_method_return(msg, "");
            } else {
                sd_bus_reply_method_errorf(msg, "org.bluez.Error.Rejected", "");
            }
        } else {
            if(std::holds_alternative<std::string>(response)) {
                sd_bus_reply_method_return(msg, "s", std::get<std::string>(response).c_str());
            } else {
                sd_bus_reply_method_return(msg, "u", std::get<int>(response));
            }
        }
        sd_bus_message_unref(msg);
    }
}

