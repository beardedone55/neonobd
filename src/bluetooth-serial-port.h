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
#include <map>
#include <shared_mutex>

class BluetoothSerialPort : public HardwareInterface {
    public:
        BluetoothSerialPort();
        BluetoothSerialPort(const BluetoothSerialPort&) = delete;
        void operator=(const BluetoothSerialPort&) = delete;
        virtual ~BluetoothSerialPort();

        //HardwareInterface overrides
        bool connect(const sigc::slot<void(bool)>&  connect_complete,
                     const sigc::slot<void(Glib::ustring, ResponseType, const void*)>&  user_prompt) override;
        void respond_from_user(const Glib::VariantBase&  response,
                               const Glib::RefPtr<Glib::Object>&  signal_handle) override;
        std::vector<char>::size_type read(std::vector<char>&  buf, 
                                          std::vector<char>::size_type buf_size = 1024,
                                          HardwareInterface::Flags flags = HardwareInterface::FLAGS_NONE) override;
        std::vector<char>::size_type write(const std::vector<char>&  buf) override;
        std::size_t read(std::string&  buf,
                         std::size_t buf_size = 1024,
                         HardwareInterface::Flags flags = HardwareInterface::FLAGS_NONE) override;
        std::size_t write(const std::string&  buf) override;

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
        using ProxyMap = std::map<Glib::ustring, Glib::RefPtr<Gio::DBus::Proxy>>;
        using ObjectManagerPtr = Glib::RefPtr<Gio::DBus::ObjectManagerClient>;
        using MethodInvocationPtr = Glib::RefPtr<Gio::DBus::MethodInvocation>;

        ProxyMap controllers;
        ProxyMap remoteDevices;
        Glib::RefPtr<Gio::DBus::Proxy> selected_controller;
        Glib::RefPtr<Gio::DBus::Proxy> agentManager;
        Glib::RefPtr<Gio::DBus::Proxy> profileManager;
        ObjectManagerPtr manager;
        sigc::signal<void(int)> probe_progress_signal;
        bool probe_in_progress;
        int probe_progress;
        Gio::DBus::InterfaceVTable agent_vtable; 
        Gio::DBus::InterfaceVTable profile_vtable;
        int sock_fd;
        std::shared_mutex sock_fd_mutex;
        Glib::DBusObjectPathString connected_device_path;
        sigc::signal<void(Glib::ustring, Glib::VariantType, Glib::RefPtr<void>)> request_user_input;
        sigc::signal<void()> agent_cancel;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_pin_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_passkey_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_confirmation_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_authorization_invocation;
        //Private Methods
        void manager_created(Glib::RefPtr<Gio::AsyncResult>& result);
        void update_object_state(const Glib::RefPtr<Gio::DBus::Object>&  obj,
                                 bool addObject);
        void probe_finish(Glib::RefPtr<Gio::AsyncResult>& result, 
                          unsigned int timeout,
                          const Glib::RefPtr<Gio::DBus::Proxy>& controller);
        bool update_probe_progress();
        void stop_probe();
        void stop_probe_finish(Glib::RefPtr<Gio::AsyncResult>& result,
                               Glib::RefPtr<Gio::DBus::Proxy>& controller);
        void emit_probe_progress(int percentComplete);
        std::vector<Glib::ustring> get_property_values(
                          const ProxyMap& proxy_list,
                          const Glib::ustring& property_name);
        Glib::ustring get_property_value(
                          const Glib::RefPtr<Gio::DBus::Proxy>& proxy,
                          const Glib::ustring & property_name);
        Glib::RefPtr<Gio::DBus::Proxy> get_interface(const Glib::RefPtr<Gio::DBus::Object> & obj, const Glib::ustring & name);
        /*bool update_object_list(const Glib::RefPtr<Gio::DBus::Object>& obj,
                                ProxyMap& proxy_map,
                                const Glib::ustring& interface_name,
                                Glib::ustring(*name_func)(const Glib::RefPtr<Gio::DBus::Proxy>&),
                                bool addObject);*/
        void update_default_interface(const Glib::RefPtr<Gio::DBus::Object>& obj,
                                      Glib::RefPtr<Gio::DBus::Proxy>& default_interface,
                                      const Glib::ustring&  interface_name,
                                      bool addObject);
        static Glib::ustring controller_name(const Glib::RefPtr<Gio::DBus::Proxy>& proxy);
        static Glib::ustring device_name(const Glib::RefPtr<Gio::DBus::Proxy>& proxy);
        void register_profile();
        void register_agent();
        void register_complete(Glib::RefPtr<Gio::AsyncResult>& result,
                               Glib::RefPtr<Gio::DBus::Proxy>& manager);
        void register_object(const char *interface_definition,
                             const Gio::DBus::InterfaceVTable & vtable);
        void
        agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                     const Glib::ustring&  sender,
                     const Glib::ustring&  object_path,
                     const Glib::ustring&  interface_name,
                     const Glib::ustring&  method_name,
                     const Glib::VariantContainerBase& parameters,
                     const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
        void
        request_pairing_info(const Glib::VariantContainerBase& parameters,
                             const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation,
                             Glib::RefPtr<Gio::DBus::MethodInvocation>& saved_invocation,
                             sigc::signal<void(Glib::ustring)> & signal);
        void export_agent();

        void
        profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                       const Glib::ustring&  sender,
                       const Glib::ustring&  object_path,
                       const Glib::ustring&  interface_name,
                       const Glib::ustring&  method_name,
                       const Glib::VariantContainerBase& parameters,
                       const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
        void export_profile();
        template <typename T>
        void dbus_return(Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation,
                         const T& return_value);
        void dbus_confirm_request(bool confirmed,
                                  Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

        void request_from_user(const Glib::ustring& message,
                               const std::string& responseType,
                               const MethodInvocationPtr& invocation);
};

