#include "bluetooth-serial-port.h"
#include <unistd.h>
#include <iostream>
#include <mutex>
#include <iomanip>
#include <sys/socket.h>

BluetoothSerialPort::BluetoothSerialPort() :
    probe_in_progress{false},
    sock_fd{-1},
    profile_vtable{sigc::mem_fun(*this,
                        &BluetoothSerialPort::profile_method)},
    agent_vtable{sigc::mem_fun(*this,
                        &BluetoothSerialPort::agent_method)}
{
    //Create BluetoothSerialPort Object Manager
    Gio::DBus::ObjectManagerClient::create_for_bus(
            Gio::DBus::BusType::SYSTEM,
            "org.bluez", "/", sigc::mem_fun(*this, &BluetoothSerialPort::manager_created));
}

BluetoothSerialPort::~BluetoothSerialPort()
{
    std::unique_lock lock(sock_fd_mutex);
    if(sock_fd >= -1)
        close(sock_fd);
}

bool BluetoothSerialPort::connect(const sigc::slot<void(bool)> & connect_complete,
                                  const sigc::slot<void(Glib::ustring, ResponseType, const void*)> & user_prompt)
{
    return false;
}

std::vector<char>::size_type 
BluetoothSerialPort::read(std::vector<char> & buf,
                          std::vector<char>::size_type buf_size,
                          HardwareInterface::Flags flags)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        buf.resize(buf_size);
        return recv(sock_fd, buf.data(), buf_size * sizeof(char), 0);
    }
    return 0;
}

std::vector<char>::size_type BluetoothSerialPort::write(const std::vector<char> & buf)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        return send(sock_fd, buf.data(), buf.size() * sizeof(char), 0);
    }
    return 0;
}

std::size_t 
BluetoothSerialPort::read(std::string & buf,
                          std::size_t buf_size,
                          HardwareInterface::Flags flags)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        buf.resize(buf_size);
        return recv(sock_fd, buf.data(), buf.size(), 0);
    }
    return 0;
}

std::size_t BluetoothSerialPort::write(const std::string & buf)
{
    std::shared_lock lock(sock_fd_mutex);
    if (sock_fd >= 0)
    {
        return send(sock_fd, buf.data(), buf.size(), 0);
    }
    return 0;
}

#define add_object(obj) add_remove_object(obj, true)

void BluetoothSerialPort::manager_created(AsyncResultPtr & result)
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
                sigc::bind(sigc::mem_fun(*this,&BluetoothSerialPort::add_remove_object),
                           true));
        manager->signal_object_removed().connect(
                sigc::bind(sigc::mem_fun(*this,&BluetoothSerialPort::add_remove_object),
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

BluetoothSerialPort::ProxyPtr
BluetoothSerialPort::get_interface(const DBusObjectPtr &obj,
                         const Glib::ustring &name)
{
    return std::dynamic_pointer_cast<Gio::DBus::Proxy>(obj->get_interface(name));
}

bool BluetoothSerialPort::update_object_list(const DBusObjectPtr &obj,
                                   ProxyMap &proxy_map,
                                   const Glib::ustring &interface_name,
                                   Glib::ustring(*name_func)(const ProxyPtr&),
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

Glib::ustring BluetoothSerialPort::controller_name(const ProxyPtr &proxy)
{
    //controllers will be identified by the object path for now
    return proxy->get_object_path(); 
} 

Glib::ustring BluetoothSerialPort::device_name(const ProxyPtr &proxy)
{
    //Each device will be identified by its address,
    //since it is guaranteed to be unique.
    Glib::Variant<Glib::ustring> address;
    proxy->get_cached_property(address, "Address");
    return address.get();
} 

void BluetoothSerialPort::update_default_interface(const DBusObjectPtr &obj,
                                         ProxyPtr &default_interface,
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

void BluetoothSerialPort::add_remove_object(const DBusObjectPtr & obj,
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

bool BluetoothSerialPort::select_controller(const Glib::ustring &controller_name)
{
    if(controllers.count(controller_name))
    {
        selected_controller = controllers[controller_name];
        return true;
    }

    return false;
}

std::vector<Glib::ustring> BluetoothSerialPort::get_controller_names()
{
    std::vector<Glib::ustring> ret;

    ret.reserve(controllers.size());
    for(auto& controller : controllers)
        ret.emplace_back(controller.first);

    return ret;
}

void BluetoothSerialPort::emit_probe_progress(int percentComplete)
{
    if(percentComplete == 100)
        probe_in_progress = false;
    probe_progress_signal.emit(percentComplete);
}

void BluetoothSerialPort::stop_probe_finish(AsyncResultPtr & result,
                                  ProxyPtr controller)
{
    try { controller->call_finish(result); }
    catch(Glib::Error e) { error(e.what()); }

    emit_probe_progress(100);
}

void BluetoothSerialPort::stop_probe()
{
    //Timeout occurred; stop probing devices.
    if(selected_controller)
    {
        selected_controller->call("StopDiscovery",
                                  sigc::bind(
                                      sigc::mem_fun(*this, &BluetoothSerialPort::stop_probe_finish),
                                      selected_controller));
    }
    else
        emit_probe_progress(100);
}

bool BluetoothSerialPort::update_probe_progress()
{
    emit_probe_progress(probe_progress++);
    if(probe_progress == 100)
    {
        stop_probe();
        return false;
    }
    return true;
}

void BluetoothSerialPort::probe_finish(AsyncResultPtr & result,
                             unsigned int timeout,
                             ProxyPtr controller)
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
                                            &BluetoothSerialPort::update_probe_progress),
                                   timeout * 1000/100);
}

void BluetoothSerialPort::probe_remote_devices(unsigned int probeTime)
{
    if(probe_in_progress)
        return;
    
    if(selected_controller)
    {
        probe_in_progress = true;
        selected_controller->call("StartDiscovery",
                                  sigc::bind(sigc::mem_fun(*this, &BluetoothSerialPort::probe_finish),
                                             probeTime,
                                             selected_controller));
    } 
    else 
    {
        error("No bluetooth controller selected.");
        emit_probe_progress(100);  //Cannot probe devices
    }

}

sigc::signal<void(int)> BluetoothSerialPort::signal_probe_progress()
{
    return probe_progress_signal;
}

std::map<Glib::ustring, Glib::ustring> 
BluetoothSerialPort::get_device_names_addresses()
{
    std::map<Glib::ustring, Glib::ustring> ret;

    for(auto &[ address, d ] : remoteDevices)
        ret[address] = get_property_value(d, "Alias");

    return ret;
}

Glib::ustring 
BluetoothSerialPort::get_property_value(const ProxyPtr &proxy,
                              const Glib::ustring &property_name)
{
    Glib::Variant<Glib::ustring> property;

    proxy->get_cached_property(property, property_name);

    return property.get();
}

std::vector<Glib::ustring> 
BluetoothSerialPort::get_property_values(
            const ProxyMap &proxy_list,
            const Glib::ustring &property_name)
{
    std::vector<Glib::ustring> ret;

    ret.reserve(proxy_list.size());

    for (auto &[name, proxy] : proxy_list)
        ret.push_back(get_property_value(proxy, property_name));

    return ret;
}


std::vector<Glib::ustring> BluetoothSerialPort::get_device_names()
{
    return get_property_values(remoteDevices, "Alias");
}

std::vector<Glib::ustring> BluetoothSerialPort::get_device_addresses()
{
    return get_property_values(remoteDevices, "Address");
}

void BluetoothSerialPort::error(const Glib::ustring &err_msg)
{
    std::cout << err_msg << std::endl;
}

constexpr auto OBJECT_PATH = "/com/github/beardedone55/bluetooth_serial";

//Bluetooth profile UUID for Serial Port Profile (SPP)
//See https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/
constexpr auto SERIAL_PORT_UUID = "00001101-0000-1000-8000-00805f9b34fb";

void BluetoothSerialPort::register_complete(AsyncResultPtr & result,
                                  ProxyPtr manager)
{
    //RegisterAgent and RegisterProfile don't
    //return anything.  This just checks if there were
    //errors.
    try {manager->call_finish(result);}
    catch(Glib::Error e) {error(e.what());}
}

void BluetoothSerialPort::register_profile()
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
                         sigc::bind(sigc::mem_fun(*this, &BluetoothSerialPort::register_complete),
                                    profileManager),
                         parameters);
}

void BluetoothSerialPort::register_agent()
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
                       sigc::bind(sigc::mem_fun(*this, &BluetoothSerialPort::register_complete),
                                  agentManager),
                       parameters);
}

void BluetoothSerialPort::register_object(const char *interface_definition,
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
"        <arg type='q' name='device' direction='out'/>"
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
BluetoothSerialPort::agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                        const Glib::ustring & sender,
                        const Glib::ustring & object_path,
                        const Glib::ustring & interface_name,
                        const Glib::ustring & method_name,
                        const Glib::VariantContainerBase& parameters,
                        const MethodInvocationPtr & invocation)
{
    if(method_name == "RequestPinCode")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        request_from_user("Enter Pin Code for " + device_path.get() + ".", "s", invocation);
        return;
    }

    if(method_name == "RequestPasskey")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        request_from_user("Enter Passkey (0-9999999) for " + device_path.get() + ".", "q", invocation);
        return;
    }

    if(method_name == "DisplayPinCode")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<Glib::ustring> pin_code;
        parameters.get_child(pin_code, 1);

        request_from_user("Pin Code for " + device_path.get() + " is " + pin_code.get() + ".", "", {});
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

        request_from_user("Passkey for " + device_path.get() + " is " + Glib::ustring::format(std::setfill(L'0'),std::setw(6), pass_key.get()) + ".", "", {});
        invocation->return_value({});
        return;
    }

    if(method_name == "RequestConfirmation")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<guint32> pass_key;
        parameters.get_child(pass_key, 1);
        request_from_user("Confirm Passkey for " + device_path.get() + " is " + Glib::ustring::format(std::setw(6), pass_key.get()) + ".", "b", invocation);

        return;
    }

    if(method_name == "RequestAuthorization")
    {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        request_from_user("Authorize Connection to Device " + device_path.get() + "?", "b", invocation);
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
        request_from_user("", "", {});
        invocation->return_value({});
    }
}

void
BluetoothSerialPort::request_from_user(const Glib::ustring & message,
                                       std::string responseType,
                                       const MethodInvocationPtr & invocation)
{
    //This message emits a signal that should solicit a response from the
    //user. It does the work for "RequestPinCode", "RequestPasskey",
    //"RequestAuthorization", and "RequestConfirmation".

    if(!request_user_input.empty())
    {

        request_user_input.emit(message, Glib::VariantType(responseType), invocation);
        //User is responsible for invoking respond_from_user()
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

void BluetoothSerialPort::respond_from_user(const Glib::VariantBase & response,
                                            const Glib::RefPtr<Glib::Object> & signal_handle)
{
    const auto invocation = std::dynamic_pointer_cast<Gio::DBus::MethodInvocation>(signal_handle);
    if(invocation)
    {
        if(response && response.is_of_type(Glib::VariantType("b")))
        {
            auto & bool_response = Glib::VariantBase::cast_dynamic<const Glib::Variant<bool>>(response);
            if(bool_response && bool_response.get())
            {
                invocation->return_value({});
            }
            else
            {
                Gio::DBus::Error error(Gio::DBus::Error::FAILED, "org.bluez.Error.Rejected");
                invocation->return_error(error);
            }
        }
        else
        {
            invocation->return_value(Glib::VariantContainerBase::create_tuple(response));
        }
    }
}

void BluetoothSerialPort::export_agent()
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
BluetoothSerialPort::profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
                          const Glib::ustring & sender,
                          const Glib::ustring & object_path,
                          const Glib::ustring & interface_name,
                          const Glib::ustring & method_name,
                          const Glib::VariantContainerBase& parameters,
                          const MethodInvocationPtr & invocation)
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
            {
                std::unique_lock lock(sock_fd_mutex);
                sock_fd = fd_list->get(fd_index.get());
            }
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
            {
                std::unique_lock lock(sock_fd_mutex);
                close(sock_fd);
                sock_fd = -1;
            }
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

void BluetoothSerialPort::export_profile()
{
    register_object(Profile1_definition, profile_vtable);
}

