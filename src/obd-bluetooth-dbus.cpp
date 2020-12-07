#include "obd-bluetooth-dbus.h"
#include <unistd.h>
#include <iostream>

#ifdef GTK4
#define BUS_TYPE_SYSTEM SYSTEM
#endif

BlueTooth::BlueTooth() :
    probe_in_progress{false},
    sock_fd{-1},
    profile_vtable{sigc::mem_fun(*this,
                        &BlueTooth::profile_method)},
    agent_vtable{sigc::mem_fun(*this,
                        &BlueTooth::agent_method)}
{
    //Create BlueTooth Object Manager
    Gio::DBus::ObjectManagerClient::create_for_bus(
            Gio::DBus::BusType::BUS_TYPE_SYSTEM,
            "org.bluez", "/", sigc::mem_fun(*this, &BlueTooth::manager_created));
}

BlueTooth::~BlueTooth()
{
    if(sock_fd >= -1)
        close(sock_fd);
}

#define add_object(obj) add_remove_object(obj, true)

void BlueTooth::manager_created(AsyncResult & result)
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

void BlueTooth::stop_probe_finish(AsyncResult & result,
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

void BlueTooth::probe_finish(AsyncResult & result,
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

constexpr auto OBJECT_PATH = "/org/obd_scanner/serial";
constexpr auto SERIAL_PORT_UUID = "00001101-0000-1000-8000-00805f9b34fb";

void BlueTooth::register_complete(AsyncResult & result,
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
    auto uuid = Glib::Variant<Glib::ustring>::create(SERIAL_PORT_UUID);
    register_profile_params.push_back(uuid);

    //Dict {string, variant} options:
    //   Name = "obd-serial"
    //   Service = SERIAL_PORT_UUID
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

void BlueTooth::register_object(const char *interface_definition,
                                const Gio::DBus::InterfaceVTable &vtable)
{
    if(manager)
    {
        auto node = Gio::DBus::NodeInfo::create_for_xml(interface_definition);
        manager->get_connection()->register_object(OBJECT_PATH,
                                                   node->lookup_interface(),
                                                   vtable);
    }
}

constexpr auto Agent1_definition =
"<node>"
"  <interface name='org.bluez.Agent1'>"
"     <method name='Release'>"
"     </method>"
"     <method name='RequestPinCode'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='pincode' direction='out'/>"
"     </method>"
"     <method name='DisplayPinCode'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='pincode' direction='in'/>"
"     </method>"
"     <method name='RequestPasskey'>"
"        <arg type='o' name='device' direction='in'/>"
"     </method>"
"     <method name='DisplayPasskey'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='u' name='passkey' direction='in'/>"
"        <arg type='q' name='entered' direction='in'/>"
"     </method>"
"     <method name='RequestConfirmation'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='u' name='passkey' direction='in'/>"
"     </method>"
"     <method name='RequestAuthorization'>"
"        <arg type='o' name='device' direction='in'/>"
"     </method>"
"     <method name='AuthorizeService'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='uuid' direction='in'/>"
"     </method>"
"     <method name='Cancel'>"
"     </method>"
"  </interface>"
"</node>";

void
BlueTooth::agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                        const Glib::ustring & sender,
                        const Glib::ustring & object_path,
                        const Glib::ustring & interface_name,
                        const Glib::ustring & method_name,
                        const Glib::VariantContainerBase& parameters,
                        const MethodInvocation & invocation)
{
    if(method_name == "RequestPinCode")
    {
        request_pairing_info(parameters, invocation, request_pin_invocation, request_pin_code);
        //User that connected slot to request_pin_code must call send_pin_code().
        return;
    }
    if(method_name == "RequestPasskey")
    {
        request_pairing_info(parameters, invocation, request_passkey_invocation, request_pass_key);
        //User that connected slot to request_pass_key must call send_pass_key().
        return;
    }
    if(method_name == "DisplayPinCode")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<Glib::ustring> pin_code;
        parameters.get_child(pin_code, 1);

        display_pin_code.emit(device_path.get(), pin_code.get());
        invocation->return_value({});
        return;
    }
    if(method_name == "DisplayPasskey")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<guint32> pass_key;
        parameters.get_child(pass_key, 1);

        Glib::Variant<guint16> entered;
        parameters.get_child(entered, 2);

        display_pass_key.emit(device_path.get(), pass_key.get(), entered.get());
        invocation->return_value({});
        return;
    }
    if(method_name == "RequestConfirmation")
    {
        if(!request_confirmation.empty())
        {
            //Grab reference to MethodInvocation.
            //It will be used when user responds to request.
            request_confirmation_invocation = invocation;

            Glib::Variant<Glib::DBusObjectPathString> device_path;
            parameters.get_child(device_path, 0);

            Glib::Variant<guint32> pass_key;
            parameters.get_child(pass_key, 1);

            request_confirmation.emit(device_path.get(), pass_key.get());
            //User is responsible for invoking send_confirmation()
            //to complete operation.
        }
        else
        {
            //No one has registered with the signal.
            //Return an error.
            Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }
        return;
    }
    if(method_name == "RequestAuthorization")
    {
        request_pairing_info(parameters, invocation,
                             request_authorization_invocation,
                             request_authorization);
        //User that connected slot to request_authorization must
        //call send_authorization()
        return;
    }
    if(method_name == "AuthorizeService")
    {
        //I don't know what this is supposed to do, so we'll just
        //return an error for now.
        Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
        invocation->return_error(error);
    }
    if(method_name == "Cancel")
    {
        agent_cancel.emit();
        invocation->return_value({});
    }
}

void
BlueTooth::request_pairing_info(const Glib::VariantContainerBase & parameters,
                                const MethodInvocation & invocation,
                                MethodInvocation & saved_invocation,
                                sigc::signal<void, Glib::ustring> &signal)
{
    //This function does the work for "RequestPinCode", "RequestPasskey",
    //and "RequestAuthorization".  It is not used for "RequestConfirmation"
    //because that method has to pass a passkey along with the object
    //path, so the user can verify it.

    if(!signal.empty())
    {
        //Grab reference to MethodInvocation.
        //It will be used when user responds to request.
        saved_invocation = invocation;

        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        signal.emit(device_path.get());
        //User is responsible for invoking response method (e.g., send_pin_code)
        //to complete operation.
    }
    else
    {
        //No one has registered with the signal.
        //Return an error.
        Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
        invocation->return_error(error);
    }
}

void BlueTooth::send_pin_code(const Glib::ustring &pin_code)
{
    dbus_return(request_pin_invocation, pin_code);
}

void BlueTooth::send_pass_key(guint32 pass_key)
{
    dbus_return(request_passkey_invocation, pass_key);
}

void BlueTooth::dbus_confirm_request(bool confirmed,
                                     MethodInvocation & invocation)
{
    if(invocation)
    {
        if(confirmed)
            invocation->return_value({});
        else
        {
            Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }
        invocation.reset();
    }
}

void BlueTooth::send_confirmation(bool confirmed)
{
    dbus_confirm_request(confirmed, request_confirmation_invocation);
}

void BlueTooth::send_authorization(bool authorized)
{
    dbus_confirm_request(authorized, request_authorization_invocation);
}

template <typename T>
void BlueTooth::dbus_return(MethodInvocation & invocation,
                            const T &return_value)
{
    if(invocation) //If we are expecting to return something
    {
        auto ret = Glib::VariantContainerBase::create_tuple(
                      Glib::Variant<T>::create(return_value));

        invocation->return_value(ret);
        invocation.reset(); //Drop reference to MethodInvocation
    }
}

void BlueTooth::export_agent()
{
    register_object(Agent1_definition, agent_vtable);
}

constexpr auto Profile1_definition =
"<node>"
"  <interface name='org.bluez.Profile1'>"
"    <method name='Release'>"
"    </method>"
"    <method name='NewConnection'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='h' name='fd' direction='in'/>"
"      <arg type='a{sv}' name='fd_properties' direction='in'/>"
"    </method>"
"    <method name='RequestDisconnection'>"
"      <arg type='o' name='device' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";


void
BlueTooth::profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                          const Glib::ustring & sender,
                          const Glib::ustring & object_path,
                          const Glib::ustring & interface_name,
                          const Glib::ustring & method_name,
                          const Glib::VariantContainerBase& parameters,
                          const MethodInvocation & invocation)
{

    if(method_name == "NewConnection")
    {
        //Parameters:
        //   device
        //   fd
        //   fd_properties

        //1st parameter is object path
        Glib::Variant<Glib::DBusObjectPathString> obj_path;
        parameters.get_child(obj_path, 0);
        connected_device_path = obj_path.get();

        //2nd parameter is fd index
        Glib::Variant<guint32> fd_index;
        parameters.get_child(fd_index, 1);

        //get socket fd from unix fd list
        auto fd_list = invocation->get_message()->get_unix_fd_list();
        if(sock_fd < 0)
        {
            //Grab the socket, so we can communicate with
            //device, and return to acknowlege connection.
            sock_fd = fd_list->get(fd_index.get());

            //Returns: void
            invocation->return_value({});
        }
        else //We are already connected to a device (Shouldn't happen...)
        {
            //Close the new socket and return an error.
            close(fd_list->get(fd_index.get()));
            Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }

        return;
    }

    if(method_name == "RequestDisconnection")
    {
        //Parameters:
        //  device
        Glib::Variant<Glib::DBusObjectPathString> obj_path;
        parameters.get_child(obj_path, 0);

        if(sock_fd >= 0 && obj_path.get() == connected_device_path)
        {
            close(sock_fd);
            sock_fd = -1;

            //Returns void
            invocation->return_value({});
        }
        else //Disconnect requested for unknown device
        {
            //Return an error
            Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }
        return;
    }

    if(method_name == "Release")
    {
        //Profile was removed by profile manager.
        //Some cleanup could be done, but for now, just ignore.
        //No response is expected.
        return;
    }

    //Unexpected method, return an error (Shouldn't happen)....
    Gio::DBus::Error error(Gio::DBus::Error::UNKNOWN_METHOD, "org.bluez.Error.NotSupported");
    invocation->return_error(error);
}

void BlueTooth::export_profile()
{
    register_object(Profile1_definition, profile_vtable);
}

