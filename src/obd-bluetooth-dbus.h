#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <giomm.h>

class BlueTooth {
    public:
        BlueTooth();
        ~BlueTooth();

        //Host Controller Access Methods
        std::vector<Glib::ustring> get_controller_names();
        bool select_controller(const Glib::ustring &controller_name);

        using ProxyMap = std::map<Glib::ustring, Glib::RefPtr<Gio::DBus::Proxy>>;
        using Proxy = Glib::RefPtr<Gio::DBus::Proxy>;
        using ObjectManager = Glib::RefPtr<Gio::DBus::ObjectManagerClient>;
        using DBusObject = Glib::RefPtr<Gio::DBus::Object>;

        //Remote Device Access Methods
        std::vector<Glib::ustring> get_device_addresses();
        std::vector<Glib::ustring> get_device_names();
        std::map<Glib::ustring, Glib::ustring> get_device_names_addresses();
        sigc::signal<void,int> signal_probe_progress();
        void probe_remote_devices(unsigned int probeTime = 10);
    private:
        ProxyMap controllers;
        ProxyMap remoteDevices;
        Proxy selected_controller;
        ObjectManager manager;
        sigc::signal<void,int> probe_progress_signal;
        bool probe_in_progress;
        int probe_progress;
        
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
        static Glib::ustring controller_name(const Proxy &proxy);
        static Glib::ustring device_name(const Proxy &proxy);
};

