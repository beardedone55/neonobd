#include "obd-bluetooth-dbus.h"
#include <unistd.h>
#include <iostream>

#ifdef GTK4
#define BUS_TYPE_SYSTEM SYSTEM
#endif

BlueTooth::BlueTooth() :
    probe_in_progress{false}
{
    //Create BlueTooth Object Manager
    Gio::DBus::ObjectManagerClient::create_for_bus(
            Gio::DBus::BusType::BUS_TYPE_SYSTEM,
            "org.bluez", "/", sigc::mem_fun(*this, &BlueTooth::manager_created));
}

BlueTooth::~BlueTooth()
{
}

#define add_object(obj) add_remove_object(obj, true)

void BlueTooth::manager_created(Glib::RefPtr<Gio::AsyncResult> &result)
{
    try
    {
        manager = Gio::DBus::ObjectManagerClient::create_for_bus_finish(result);
    }
    catch(Glib::Error e)
    {
        error(e.what());
        return;
    }
    if(manager)
    {
        manager->signal_object_added().connect(
                sigc::bind<bool>(sigc::mem_fun(*this,&BlueTooth::add_remove_object),
                                 true));
        manager->signal_object_removed().connect(
                sigc::bind<bool>(sigc::mem_fun(*this,&BlueTooth::add_remove_object),
                                 false));

        auto objects = manager->get_objects();
        for(auto &object : objects)
            add_object(object);
    }
    //export_profile();
    //export_agent();
    register_profile();
    register_agent();
}

BlueTooth::Proxy
BlueTooth::get_interface(const DBusObject &obj,
                         const Glib::ustring &name)
{
#ifdef GTK4
    return std::dynamic_pointer_cast<Gio::DBus::Proxy>(obj->get_interface(name));
#else
    return Glib::wrap((GDBusProxy *)g_dbus_object_get_interface(obj->gobj(), name.c_str()));
#endif
}

bool BlueTooth::update_object_list(const DBusObject &obj,
                                   ProxyMap &proxy_map,
                                   const Glib::ustring &interface_name,
                                   Glib::ustring(*name_func)(const Proxy&),
                                   bool addObject)
{
    auto interface = get_interface(obj, interface_name);
    if(interface)
    {
        if(addObject)
            proxy_map[name_func(interface)] = interface;
        else
            proxy_map.erase(name_func(interface));
        return true;
    }
    return false;
}

Glib::ustring BlueTooth::controller_name(const Proxy &proxy)
{
    //controllers will be identified by the object path for now
    return proxy->get_object_path(); 
} 

Glib::ustring BlueTooth::device_name(const Proxy &proxy)
{
    //Each device will be identified by its address,
    //since it is guaranteed to be unique.
    Glib::Variant<Glib::ustring> address;
    proxy->get_cached_property(address, "Address");
    return address.get();
} 

void BlueTooth::update_default_interface(const DBusObject &obj,
                                         Proxy &default_interface,
                                         const Glib::ustring & interface_name,
                                         bool addObject)
{
    auto interface = get_interface(obj, interface_name);
    if(get_interface(obj, interface_name))
    {
        if(addObject)
            default_interface = interface;
        else
            default_interface.reset();
    }
}

void BlueTooth::add_remove_object(const DBusObject & obj,
                                  bool addObject)
{
    //Object was added or removed.  Check if it is an adapter
    //or device and modify the appropriate set.
    if(!update_object_list(obj, controllers, "org.bluez.Adapter1", controller_name, addObject))
        update_object_list(obj, remoteDevices, "org.bluez.Device1", device_name, addObject);

    //We also need to save away the agent manager and profile manager when they appear.
    update_default_interface(obj, agentManager, "org.bluez.AgentManager1", addObject);
    update_default_interface(obj, profileManager, "org.bluez.ProfileManager1", addObject);
}

bool BlueTooth::select_controller(const Glib::ustring &controller_name)
{
    if(controllers.count(controller_name))
    {
        selected_controller = controllers[controller_name];
        return true;
    }

    return false;
}

std::vector<Glib::ustring> BlueTooth::get_controller_names()
{
    std::vector<Glib::ustring> ret;

    ret.reserve(controllers.size());
    for(auto& controller : controllers)
        ret.emplace_back(controller.first);

    return ret;
}

void BlueTooth::emit_probe_progress(int percentComplete)
{
    if(percentComplete == 100)
        probe_in_progress = false;
    probe_progress_signal.emit(percentComplete);
}

void BlueTooth::stop_probe_finish(Glib::RefPtr<Gio::AsyncResult>& result,
                                  Proxy controller)
{
    try { controller->call_finish(result); }
    catch(Glib::Error e) { error(e.what()); }

    emit_probe_progress(100);
}

void BlueTooth::stop_probe()
{
    //Timeout occurred; stop probing devices.
    if(selected_controller)
    {
        selected_controller->call("StopDiscovery",
                                  sigc::bind<Proxy>(
                                      sigc::mem_fun(*this, &BlueTooth::stop_probe_finish),
                                      selected_controller));
    }
    else
        emit_probe_progress(100);
}

bool BlueTooth::update_probe_progress()
{
    emit_probe_progress(probe_progress++);
    if(probe_progress == 100)
    {
        stop_probe();
        return false;
    }
    return true;
}

void BlueTooth::probe_finish(Glib::RefPtr<Gio::AsyncResult>& result, 
                             unsigned int timeout,
                             Proxy controller)
{
    //StartDiscovery command issued; now wait for timeout
    try { controller->call_finish(result); }
    catch(Glib::Error e)
    {
        error(e.what());
        emit_probe_progress(100);
        return;
    }

    //Convert timeout to milliseconds, and interrupt every time
    //we are 100th the way to completion, hence the *1000/100
    probe_progress = 0;
    Glib::signal_timeout().connect(sigc::mem_fun(*this, 
                                            &BlueTooth::update_probe_progress),
                                   timeout * 1000/100);
}

void BlueTooth::probe_remote_devices(unsigned int probeTime)
{
    if(probe_in_progress)
        return;
    
    if(selected_controller)
    {
        probe_in_progress = true;
        selected_controller->call("StartDiscovery",
                                  sigc::bind<unsigned int, Proxy>(
                                      sigc::mem_fun(*this, &BlueTooth::probe_finish),
                                      probeTime,
                                      selected_controller));
    } 
    else 
    {
        error("No bluetooth controller selected.");
        emit_probe_progress(100);  //Cannot probe devices
    }

}

sigc::signal<void,int> BlueTooth::signal_probe_progress()
{
    return probe_progress_signal;
}

std::map<Glib::ustring, Glib::ustring> 
BlueTooth::get_device_names_addresses()
{
    std::map<Glib::ustring, Glib::ustring> ret;

    for(auto &[ address, d ] : remoteDevices)
        ret[address] = get_property_value(d, "Alias");

    return ret;
}

Glib::ustring 
BlueTooth::get_property_value(const Proxy &proxy,
                              const Glib::ustring &property_name)
{
    Glib::Variant<Glib::ustring> property;

    proxy->get_cached_property(property, property_name);

    return property.get();
}

std::vector<Glib::ustring> 
BlueTooth::get_property_values(
            const ProxyMap &proxy_list,
            const Glib::ustring &property_name)
{
    std::vector<Glib::ustring> ret;

    ret.reserve(proxy_list.size());

    for (auto &[name, proxy] : proxy_list)
        ret.push_back(get_property_value(proxy, property_name));

    return ret;
}


std::vector<Glib::ustring> BlueTooth::get_device_names()
{
    return get_property_values(remoteDevices, "Alias");
}

std::vector<Glib::ustring> BlueTooth::get_device_addresses()
{
    return get_property_values(remoteDevices, "Address");
}

void BlueTooth::error(const Glib::ustring &err_msg)
{
    std::cout << err_msg << std::endl;
}

constexpr auto OBJECT_PATH = "/org/obd-scanner/serial";
constexpr auto SP_UUID = "00001101-0000-1000-8000-00805f9b34fb";

void BlueTooth::register_complete(Glib::RefPtr<Gio::AsyncResult>& result,
                                  Proxy manager)
{
    //RegisterAgent and RegisterProfile don't
    //return anything.  This just checks if there were
    //errors.
    try {manager->call_finish(result);}
    catch(Glib::Error e) {error(e.what());}
}

void BlueTooth::register_profile()
{
    if(!profileManager)
        return;

    //RegisterProfile parameters:
    //   String profile
    //   String uuid
    //   Dict   options
    std::vector<Glib::VariantBase> register_profile_params;

    //String profile
    auto profile = Glib::Variant<Glib::DBusObjectPathString>::create(OBJECT_PATH);
    register_profile_params.push_back(profile);

    //String uuid
    auto uuid = Glib::Variant<Glib::ustring>::create(SP_UUID);
    register_profile_params.push_back(uuid);

    //Dict {string, variant} options:
    //   Name = "obd-serial"
    //   Service = SP_UUID
    //   Role = "client"
    //   Channel = (uint16) 1
    //   AutoConnect = (bool) true
    std::map<Glib::ustring, Glib::VariantBase> options;
    options["Name"] = Glib::Variant<Glib::ustring>::create("obd-serial");
    options["Service"] = uuid;
    options["Role"] = Glib::Variant<Glib::ustring>::create("client");
    options["Channel"] = Glib::Variant<guint16>::create(1);
    options["AutoConnect"] = Glib::Variant<bool>::create(true);

    register_profile_params.push_back(
            Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>>::create(options));

    auto parameters = Glib::VariantContainerBase::create_tuple(register_profile_params);

    profileManager->call("RegisterProfile",
                         sigc::bind<Proxy>(
                            sigc::mem_fun(*this, &BlueTooth::register_complete),
                            profileManager),
                         parameters);
}

void BlueTooth::register_agent()
{
    if(!agentManager)
        return;
    //RegisterAgent parameters:
    //   String agent
    //   String capabilities
    auto parameters =
        Glib::VariantContainerBase::create_tuple(
            std::vector<Glib::VariantBase>(
            {
                Glib::Variant<Glib::DBusObjectPathString>::create(OBJECT_PATH),
                Glib::Variant<Glib::ustring>::create("KeyboardDisplay"),
            }));

    agentManager->call("RegisterAgent",
                       sigc::bind<Proxy>(
                           sigc::mem_fun(*this, &BlueTooth::register_complete),
                           agentManager),
                       parameters);
}
