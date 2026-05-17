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

#pragma once

#include "dbus-type.hpp"
#include "hardware-interface.hpp"
#include "neonobd_types.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <systemd/sd-bus.h>
#include <unordered_map>
#include <vector>

using neon::ResponseType;
using namespace std::chrono_literals;

class BluetoothSerialPort : public HardwareInterface {
  public:
    BluetoothSerialPort();
    BluetoothSerialPort(const BluetoothSerialPort&) = delete;
    void operator=(const BluetoothSerialPort&) = delete;
    ~BluetoothSerialPort() override;

    // HardwareInterface overrides
    bool connect(const std::string& device_address, std::function<void(bool)> callback) override;
    void respond_from_user(const ResponseVariant& response, void* handle) override;

    void set_timeout(std::chrono::milliseconds timeout) override;

    //Event Loop Processing Methods
    //-------------------------------------------------------
    void process_events(std::chrono::microseconds timeout = 0s) override;
    int get_event_fd() override;


    // Host Controller Access Methods
    //--------------------------------------------------------
    // Get vector containing bluez dbus object paths of all
    // bluetooth controllers available on the system.
    std::vector<std::string> get_controller_names();

    // Select the default controller by dbus object path.
    // This controller will be used to find and connect
    // to remote devices.
    bool select_controller(const std::string& controller_name);

    // Remote Device Access Methods
    //-------------------------------------------------

    // Disconnect remote device
    void disconnect(std::function<void()> callback);

    // Return map of remote device addresses to their device names

    struct DeviceInfo {
        std::string name;
        std::string address;
    };

    std::vector<DeviceInfo> get_device_names_addresses();

    // Initiate scan of remote devices using default
    // bluetooth controller.
    void probe_remote_devices(std::function<void(int)> callback, std::chrono::seconds probeTime = 10s);

    // Pairing response methods
    //-----------------------------------------------
    // Send pin code in response to request for pin
    // code during device pairing.  Must be called
    // by slot that is connected to request_pin_code signal.
    void send_pin_code(const std::string& pin_code);

    // Send 6-digit passkey as integer in response
    // to request for passkey during device pairing.
    // Must be called by slot that is connected to
    // request_pass_key signal.
    void send_pass_key(std::uint32_t pin_code);

    // Send confirmation (true = confirmed,
    // false = not confirmed) in response to request
    // for confirmation during device pairing.
    // Must be called by slot that has connected
    // to request_confirmation signal.
    void send_confirmation(bool confirmed);

    // Send authorization (true = confirmed,
    // false = not confirmed) in response to request
    // for authorization during device pairing.
    // Must be called by slot that has connected
    // to request_authorization signal.
    void send_authorization(bool authorized);

  private:

    using DBusPtr =
        std::unique_ptr<sd_bus, void(*)(sd_bus*)>;

    using DBusEventPtr =
        std::unique_ptr<sd_event, void(*)(sd_event*)>;

    DBusPtr m_system_bus;
    DBusEventPtr m_event;

    //Map object path to object's interfaces.
    using DBusObjects = 
        std::unordered_map<std::string, DBusType>;

    DBusObjects m_controllers;
    DBusObjects m_remote_devices;
    std::unordered_map<std::string,std::string> m_dev_name_path_map;
    std::string m_agent_manager;
    std::string m_profile_manager;

    static const sd_bus_vtable profile_vtable[];
    static const sd_bus_vtable agent_vtable[];

    std::string m_connected_device_path;
    std::string m_selected_controller;

    std::chrono::seconds m_probe_time;
    bool m_probe_in_progress = false;
    int m_probe_progress = 0;
    std::function<void(int)> m_probe_callback;
    std::function<void()> m_complete_disconnect;

    // Private Methods

    static DBusPtr get_system_dbus();
    DBusEventPtr get_dbus_event();
    void connect_object_manager();
    bool connect_object_manager(sd_bus_message_handler_t, std::string_view);
    static int add_object(sd_bus_message*, void*, sd_bus_error*);
    int add_object(const std::string& path, const DBusType& obj);
    static int remove_object(sd_bus_message*, void*, sd_bus_error*);
    int remove_object(const std::string& path, const DBusType& obj);
    void get_objects();
    static int get_objects_complete(sd_bus_message*, void*, sd_bus_error*);
    void register_profile();
    void register_agent();
    int register_object(std::string_view interface, const sd_bus_vtable vtable[]);
    static int register_complete(sd_bus_message*, void*, sd_bus_error*);

    //Profile handlers
    static int bt_release(sd_bus_message*, void*, sd_bus_error*);
    static int bt_new_connection(sd_bus_message*, void*, sd_bus_error*);
    static int bt_request_disconnection(sd_bus_message*, void*, sd_bus_error*);
   
    //Agent handlers
    static std::string format_passkey(std::uint32_t value);
    static int bt_agent_request(sd_bus_message*, void*, const std::string&, 
                                const ResponseType response_type = neon::USER_STRING);

    template<typename T>
    static int bt_agent_display(sd_bus_message*, void*, const std::string&);
    static int bt_agent_release(sd_bus_message*, void*, sd_bus_error*);
    static int bt_request_pin_code(sd_bus_message*, void*, sd_bus_error*);
    static int bt_display_pin_code(sd_bus_message*, void*, sd_bus_error*);
    static int bt_request_passkey(sd_bus_message*, void*, sd_bus_error*);
    static int bt_display_passkey(sd_bus_message*, void*, sd_bus_error*);
    static int bt_request_confirmation(sd_bus_message*, void*, sd_bus_error*);
    static int bt_request_authorization(sd_bus_message*, void*, sd_bus_error*);
    static int bt_authorize_service(sd_bus_message*, void*, sd_bus_error*);
    static int bt_agent_cancel(sd_bus_message*, void*, sd_bus_error*);

    static std::uint64_t get_tick_time(std::chrono::seconds probe_time);
    static int probe_finish(sd_bus_message*, void*, sd_bus_error*);
    static int update_probe_progress(sd_event_source *s, std::uint64_t usec, void* userdata);
    void pre_connection_scan_progress(int percent_complete, const std::string& device_address);
    int call_dbus(std::string_view path, std::string_view interface, std::string_view method,
                  sd_bus_message_handler_t callback);

    void initiate_connection(const std::string& device_address);
    static int finish_connection(sd_bus_message* reply, void* userdata, sd_bus_error*);
    void initiate_disconnect(const std::string& device_path);
    static int finish_disconnect(sd_bus_message* reply, void* userdata, sd_bus_error*);

    void stop_probe();
    static int stop_probe_finish(sd_bus_message* reply, void* userdata, sd_bus_error*);
    void emit_probe_progress(int percent_complete);

    void request_from_user(const std::string& message, const ResponseType response_type,
                           sd_bus_message* msg);

};
