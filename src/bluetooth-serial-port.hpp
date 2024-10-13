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

#pragma once

#include "connection.hpp"
#include "hardware-interface.hpp"
#include "neonobd_types.hpp"
#include <chrono>
#include <cstdint>
#include <giomm/dbusobjectmanagerclient.h>
#include <giomm/dbusproxy.h>
#include <memory>
#include <unordered_map>

using neon::ResponseType;
using namespace std::chrono_literals;

class BluetoothSerialPort : public HardwareInterface {
  public:
    BluetoothSerialPort();
    BluetoothSerialPort(const BluetoothSerialPort&) = delete;
    void operator=(const BluetoothSerialPort&) = delete;
    ~BluetoothSerialPort() override;

    // HardwareInterface overrides
    bool connect(const Glib::ustring& device_name) override;
    void respond_from_user(const Glib::VariantBase& response,
                           const Glib::RefPtr<void>& signal_handle) override;

    void set_timeout(std::chrono::milliseconds timeout) override;

    static std::shared_ptr<BluetoothSerialPort> get_BluetoothSerialPort();

    // Host Controller Access Methods
    //--------------------------------------------------------
    // Get vector containing bluez dbus object paths of all
    // bluetooth controllers available on the system.
    std::vector<Glib::ustring> get_controller_names();

    // Select the default controller by dbus object path.
    // This controller will be used to find and connect
    // to remote devices.
    bool select_controller(const Glib::ustring& controller_name);

    // Remote Device Access Methods
    //-------------------------------------------------

    // Return map of remote device addresses to their device names
    std::vector<std::pair<Glib::ustring, Glib::ustring>>
    get_device_names_addresses();

    // Signal to connect to that will report progress of
    // device scan.  Parameter is integer that represents
    // percent complete.  At 100, scan is complete.
    sigc::signal<void(int)> signal_probe_progress();

    // Initiate scan of remote devices using default
    // bluetooth controller.
    void probe_remote_devices(std::chrono::seconds probeTime = 10s);

    // Pairing response methods
    //-----------------------------------------------
    // Send pin code in response to request for pin
    // code during device pairing.  Must be called
    // by slot that is connected to request_pin_code signal.
    void send_pin_code(const Glib::ustring& pin_code);

    // Send 6-digit passkey as integer in response
    // to request for passkey during device pairing.
    // Must be called by slot that is connected to
    // request_pass_key signal.
    void send_pass_key(guint32 pin_code);

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
    using ProxyMap =
        std::unordered_map<std::string, Glib::RefPtr<Gio::DBus::Proxy>>;

    static std::weak_ptr<BluetoothSerialPort> m_bluetooth_serial_port;
    static int m_object_count;

    ProxyMap m_controllers;
    ProxyMap m_remote_devices;
    guint m_agent_id = 0;
    guint m_profile_id = 0;
    Glib::RefPtr<Gio::DBus::Proxy> m_selected_controller;
    Glib::RefPtr<Gio::DBus::Proxy> m_agent_manager;
    Glib::RefPtr<Gio::DBus::Proxy> m_profile_manager;
    Glib::RefPtr<Gio::DBus::ObjectManagerClient> m_manager;
    sigc::signal<void(int)> m_probe_progress_signal;
    Connection m_pre_connection_scan_result;
    bool m_probe_in_progress;
    int m_probe_progress;
    Glib::DBusObjectPathString m_connected_device_path;
    sigc::signal<void()> m_agent_cancel;
    Glib::RefPtr<Gio::DBus::MethodInvocation> m_request_pin_invocation;
    Glib::RefPtr<Gio::DBus::MethodInvocation> m_request_passkey_invocation;
    Glib::RefPtr<Gio::DBus::MethodInvocation> m_request_confirmation_invocation;
    Glib::RefPtr<Gio::DBus::MethodInvocation>
        m_request_authorization_invocation;
    Connection m_object_add_connection;
    Connection m_object_remove_connection;
    Connection m_probe_timer_connection;

    // Private Methods
    void manager_created(Glib::RefPtr<Gio::AsyncResult>& result);
    void update_object_state(const Glib::RefPtr<Gio::DBus::Object>& obj,
                             bool addObject);
    void add_object(const Glib::RefPtr<Gio::DBus::Object>& obj);
    void remove_object(const Glib::RefPtr<Gio::DBus::Object>& obj);
    void probe_finish(Glib::RefPtr<Gio::AsyncResult>& result,
                      std::chrono::seconds timeout,
                      const Glib::RefPtr<Gio::DBus::Proxy>& controller);
    void initiate_connection(const Glib::RefPtr<Gio::DBus::Proxy>& device);
    void finish_connection(Glib::RefPtr<Gio::AsyncResult>& result);
    bool update_probe_progress();
    void pre_connection_scan_progress(int percent_complete,
                                      const Glib::ustring& device_name);
    void stop_probe();
    void stop_probe_finish(const Glib::RefPtr<Gio::AsyncResult>& result,
                           const Glib::RefPtr<Gio::DBus::Proxy>& controller);
    void emit_probe_progress(int percent_complete);
    guint register_object(const std::string& interface_path,
                          const Gio::DBus::InterfaceVTable& vtable);
    void register_profile();
    void register_agent();
    static void
    register_complete(const Glib::RefPtr<Gio::AsyncResult>& result,
                      const Glib::RefPtr<Gio::DBus::Proxy>& manager);
    static void
    agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                 const Glib::ustring& sender, const Glib::ustring& object_path,
                 const Glib::ustring& interface_name,
                 const Glib::ustring& method_name,
                 const Glib::VariantContainerBase& parameters,
                 const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

    static void profile_method(
        const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring& sender,
        const Glib::ustring& object_path, const Glib::ustring& interface_name,
        const Glib::ustring& method_name,
        const Glib::VariantContainerBase& parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
    template <typename T>
    void dbus_return(Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation,
                     const T& return_value);
    void
    dbus_confirm_request(bool confirmed,
                         Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

    void request_from_user(
        const Glib::ustring& message, const ResponseType response_type,
        const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
};
