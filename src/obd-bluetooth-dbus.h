#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <giomm.h>

class BlueTooth {
    public:
        static BlueTooth & get_instance()
        {
            static BlueTooth bt;
            return bt;
        }

        //Host Controller Access Methods
        //--------------------------------------------------------
        //Get vector containing bluez dbus object paths of all
        //bluetooth controllers available on the system.
        std::vector<Glib::ustring> get_controller_names();

        //Select the default controller by dbus object path.
        //This controller will be used to find and connect
        //to remote devices.
        bool select_controller(const Glib::ustring &controller_name);


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
        void send_pin_code(const Glib::ustring &pin_code);

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
        using Proxy = Glib::RefPtr<Gio::DBus::Proxy>;
        using ObjectManager = Glib::RefPtr<Gio::DBus::ObjectManagerClient>;
        using DBusObject = Glib::RefPtr<Gio::DBus::Object>;
        using AsyncResult = Glib::RefPtr<Gio::AsyncResult>;
        using MethodInvocation = Glib::RefPtr<Gio::DBus::MethodInvocation>;

        BlueTooth();
        BlueTooth(const BlueTooth&) = delete;
        void operator=(const BlueTooth&) = delete;
        ~BlueTooth();
        ProxyMap controllers;
        ProxyMap remoteDevices;
        Proxy selected_controller;
        Proxy agentManager;
        Proxy profileManager;
        ObjectManager manager;
        sigc::signal<void(int)> probe_progress_signal;
        bool probe_in_progress;
        int probe_progress;
        Gio::DBus::InterfaceVTable agent_vtable; 
        Gio::DBus::InterfaceVTable profile_vtable;
        int sock_fd;
        Glib::DBusObjectPathString connected_device_path;
        sigc::signal<void(Glib::ustring)> request_pin_code;
        sigc::signal<void(Glib::ustring, Glib::ustring)> display_pin_code;
        sigc::signal<void(Glib::ustring)> request_pass_key;
        sigc::signal<void(Glib::ustring, guint32, guint16)> display_pass_key;
        sigc::signal<void(Glib::ustring, guint32)> request_confirmation;
        sigc::signal<void(Glib::ustring)> request_authorization;
        sigc::signal<void()> agent_cancel;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_pin_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_passkey_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_confirmation_invocation;
        Glib::RefPtr<Gio::DBus::MethodInvocation> request_authorization_invocation;
        //Private Methods
        void manager_created(Glib::RefPtr<Gio::AsyncResult> &result);
        void add_remove_object(const DBusObject & obj,
                               bool addObject);
        void probe_finish(Glib::RefPtr<Gio::AsyncResult>& result, 
                          unsigned int timeout,
                          Proxy controller);
        bool update_probe_progress();
        void stop_probe();
        void stop_probe_finish(Glib::RefPtr<Gio::AsyncResult>& result,
                               Proxy controller);
        void emit_probe_progress(int percentComplete);
        std::vector<Glib::ustring> get_property_values(
                          const ProxyMap &proxy_list,
                          const Glib::ustring &property_name);
        Glib::ustring get_property_value(
                          const Proxy &proxy,
                          const Glib::ustring &property_name);
        Proxy get_interface(const DBusObject &obj, const Glib::ustring &name);
        bool update_object_list(const DBusObject &obj,
                                ProxyMap &proxy_map,
                                const Glib::ustring &interface_name,
                                Glib::ustring(*name_func)(const Proxy&),
                                bool addObject);
        void update_default_interface(const DBusObject &obj,
                                      Proxy &default_interface,
                                      const Glib::ustring & interface_name,
                                      bool addObject);
        static Glib::ustring controller_name(const Proxy &proxy);
        static Glib::ustring device_name(const Proxy &proxy);
        void register_profile();
        void register_agent();
        void register_complete(Glib::RefPtr<Gio::AsyncResult>& result,
                               Proxy manager);
        void register_object(const char *interface_definition,
                             const Gio::DBus::InterfaceVTable &vtable);
        void
        agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                     const Glib::ustring & sender,
                     const Glib::ustring & object_path,
                     const Glib::ustring & interface_name,
                     const Glib::ustring & method_name,
                     const Glib::VariantContainerBase& parameters,
                     const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
        void
        request_pairing_info(const Glib::VariantContainerBase& parameters,
                             const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation,
                             Glib::RefPtr<Gio::DBus::MethodInvocation>& saved_invocation,
                             sigc::signal<void(Glib::ustring)> &signal);
        void export_agent();

        void
        profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                       const Glib::ustring & sender,
                       const Glib::ustring & object_path,
                       const Glib::ustring & interface_name,
                       const Glib::ustring & method_name,
                       const Glib::VariantContainerBase& parameters,
                       const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
        void export_profile();
        template <typename T>
        void dbus_return(Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation,
                         const T &return_value);
        void dbus_confirm_request(bool confirmed,
                                  Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation);

        void error(const Glib::ustring &err_msg);
};

