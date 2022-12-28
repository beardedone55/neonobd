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

#pragma once

#include "hardware-interface.h"
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include "neonobd_types.h"

using neon::ResponseType;

class BluetoothSerialPort : public HardwareInterface, 
                            public sigc::trackable
{
    public:
        BluetoothSerialPort(const BluetoothSerialPort&) = delete;
        void operator=(const BluetoothSerialPort&) = delete;
        virtual ~BluetoothSerialPort();

        //HardwareInterface overrides
        bool connect(const Glib::ustring& device_name) override;
        void respond_from_user(const Glib::VariantBase&  response,
                               const Glib::RefPtr<void>&  signal_handle) override;
        std::vector<char>::size_type read(std::vector<char>&  buf, 
                                          std::vector<char>::size_type buf_size = 1024,
                                          HardwareInterface::Flags flags = HardwareInterface::FLAGS_NONE) override;
        std::vector<char>::size_type write(const std::vector<char>&  buf) override;
        std::size_t read(std::string&  buf,
                         std::size_t buf_size = 1024,
                         HardwareInterface::Flags flags = HardwareInterface::FLAGS_NONE) override;
        std::size_t write(const std::string&  buf) override;

        static std::shared_ptr<BluetoothSerialPort> getBluetoothSerialPort();

        //Host Controller Access Methods
        //--------------------------------------------------------
        //Get vector containing bluez dbus object paths of all
        //bluetooth controllers available on the system.
        std::vector<Glib::ustring> get_controller_names();

        //Select the default controller by dbus object path.
        //This controller will be used to find and connect
        //to remote devices.
        bool select_controller(const Glib::ustring& controller_name);


        //Remote Device Access Methods
        //-------------------------------------------------
        //Get vecotr of addresses of all remote devices
        std::vector<Glib::ustring> get_device_addresses();

        //Get vector of names of all remote devices
        std::vector<Glib::ustring> get_device_names();

        //Return map of remote device addresses to their device names
        std::map<Glib::ustring, Glib::ustring> get_device_names_addresses();

        //Signal to connect to that will report progress of
        //device scan.  Parameter is integer that represents
        //percent complete.  At 100, scan is complete.
        sigc::signal<void(int)> signal_probe_progress();

        //Initiate scan of remote devices using default
        //bluetooth controller.
        void probe_remote_devices(unsigned int probeTime = 10);

        //Pairing response methods
        //-----------------------------------------------
        //Send pin code in response to request for pin
        //code during device pairing.  Must be called
        //by slot that is connected to request_pin_code signal.
        void send_pin_code(const Glib::ustring& pin_code);

        //Send 6-digit passkey as integer in response
        //to request for passkey during device pairing.
        //Must be called by slot that is connected to
        //request_pass_key signal.
        void send_pass_key(guint32 pin_code);

        //Send confirmation (true = confirmed,
        //false = not confirmed) in response to request
        //for confirmation during device pairing.
        //Must be called by slot that has connected
        //to request_confirmation signal.
        void send_confirmation(bool confirmed);

        //Send authorization (true = confirmed,
        //false = not confirmed) in response to request
        //for authorization during device pairing.
        //Must be called by slot that has connected
        //to request_authorization signal.
        void send_authorization(bool authorized);
    private:
        using ProxyMap = std::unordered_map<std::string, Glib::RefPtr<Gio::DBus::Proxy>>;

        static std::weak_ptr<BluetoothSerialPort> bluetoothSerialPort; 
        static Gio::DBus::InterfaceVTable agent_vtable; 
        static Gio::DBus::InterfaceVTable profile_vtable;

        ProxyMap controllers;
        ProxyMap remoteDevices;
        guint agent_id = 0;
        guint profile_id = 0;
        Glib::RefPtr<Gio::DBus::Proxy> selected_controller;
        Glib::RefPtr<Gio::DBus::Proxy> agentManager;
        Glib::RefPtr<Gio::DBus::Proxy> profileManager;
        Glib::RefPtr<Gio::DBus::ObjectManagerClient> manager;
        sigc::signal<void(int)> probe_progress_signal;
        sigc::connection preConnectionScanResult;
        bool probe_in_progress;
        int probe_progress;
        int sock_fd;
        std::shared_mutex sock_fd_mutex;
        Glib::DBusObjectPathString connected_device_path;
        sigc::signal<void()> agent_cancel;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_pin_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_passkey_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_confirmation_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_authorization_invocation;

        //Private Methods
        BluetoothSerialPort();
        void manager_created(Glib::RefPtr<Gio::AsyncResult>& result);
        void update_object_state(const Glib::RefPtr<Gio::DBus::Object>&  obj,
                                 bool addObject);
        void add_object(const Glib::RefPtr<Gio::DBus::Object>& obj);
        void remove_object(const Glib::RefPtr<Gio::DBus::Object>& obj);
        void probe_finish(Glib::RefPtr<Gio::AsyncResult>& result, 
                          unsigned int timeout,
                          const Glib::RefPtr<Gio::DBus::Proxy>& controller);
        void initiate_connection(const Glib::RefPtr<Gio::DBus::Proxy>& device);
        void finish_connection(Glib::RefPtr<Gio::AsyncResult>& result);
        bool update_probe_progress();
        void preConnectionScanProgress(int p, const Glib::ustring& device_name);
        void stop_probe();
        void stop_probe_finish(const Glib::RefPtr<Gio::AsyncResult>& result,
                               const Glib::RefPtr<Gio::DBus::Proxy>& controller);
        void emit_probe_progress(int percentComplete);
        std::vector<Glib::ustring> get_property_values(
                          const ProxyMap& proxy_list,
                          const Glib::ustring& property_name);
        Glib::ustring get_property_value(
                          const Glib::RefPtr<Gio::DBus::Proxy>& proxy,
                          const Glib::ustring & property_name);
        guint register_object(const std::string& interface_path,
                             const Gio::DBus::InterfaceVTable& vtable);
        void register_profile();
        void register_agent();
        void register_complete(const Glib::RefPtr<Gio::AsyncResult>& result,
                               const Glib::RefPtr<Gio::DBus::Proxy>& manager);
        static void agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                                 const Glib::ustring&  sender,
                                 const Glib::ustring&  object_path,
                                 const Glib::ustring&  interface_name,
                                 const Glib::ustring&  method_name,
                                 const Glib::VariantContainerBase& parameters,
                                 const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

        static void profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                                   const Glib::ustring&  sender,
                                   const Glib::ustring&  object_path,
                                   const Glib::ustring&  interface_name,
                                   const Glib::ustring&  method_name,
                                   const Glib::VariantContainerBase& parameters,
                                   const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
        template <typename T>
        void dbus_return(Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation,
                         const T& return_value);
        void dbus_confirm_request(bool confirmed,
                                  Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

        void request_from_user(const Glib::ustring& message,
                               const ResponseType responseType,
                               const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
};

