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

#include "bluetooth-serial-port.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <giomm/asyncresult.h>
#include <giomm/dbusconnection.h>
#include <giomm/dbuserror.h>
#include <giomm/dbusinterfacevtable.h>
#include <giomm/dbusintrospection.h>
#include <giomm/dbusmethodinvocation.h>
#include <giomm/dbusobject.h>
#include <giomm/dbusobjectmanagerclient.h>
#include <giomm/dbusproxy.h>
#include <giomm/resource.h>
#include <glib.h>
#include <glibmm/error.h>
#include <glibmm/main.h>
#include <glibmm/refptr.h>
#include <glibmm/variant.h>
#include <glibmm/variantdbusstring.h>
#include <glibmm/varianttype.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sigc++/signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr int FINISHED = 100;
} // namespace

std::weak_ptr<BluetoothSerialPort> BluetoothSerialPort::m_bluetooth_serial_port;

int BluetoothSerialPort::m_object_count = 0;

BluetoothSerialPort::BluetoothSerialPort()
    : m_probe_in_progress{false}, m_probe_progress{0} {
    if (m_object_count > 0) {
        throw std::runtime_error(
            "Only one instance of BluetoothSerialPort allowed.");
    }
    ++m_object_count;
    Logger::debug("Created BluetoothSerialPort.");
}

BluetoothSerialPort::~BluetoothSerialPort() {
    const std::unique_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= -1) {
        close(m_sock_fd);
    }

    if (m_manager) {
        auto connection = m_manager->get_connection();
        if (m_agent_id != 0) {
            connection->unregister_object(m_agent_id);
        }
        if (m_profile_id != 0) {
            connection->unregister_object(m_profile_id);
        }
    }

    Logger::debug << "Destroyed BluetoothSerialPort.\n";
}

void BluetoothSerialPort::finish_connection(
    Glib::RefPtr<Gio::AsyncResult>& result) {
    Logger::debug("Connection finished.");

    auto device = std::dynamic_pointer_cast<Gio::DBus::Proxy>(
        result->get_source_object_base());

    if (!device) {
        Logger::error("BluetoothSerialPort::finish_connection invalid result!");
    } else {
        try {
            auto status = device->call_finish(result);
            Logger::debug("Connect called, " + status.get_type_string() +
                          " returned.");
            m_complete_connection.emit(true);
            return;
        } catch (Glib::Error& e) {
            Logger::error("Error occurred connecting to Bluetooth Device");
            std::stringstream logstr;
            logstr << e.what() << ":" << e.code();
            Logger::error(logstr.str());
        }
    }
    m_complete_connection.emit(false);
}

void BluetoothSerialPort::initiate_connection(
    const Glib::RefPtr<Gio::DBus::Proxy>& device) {
    Logger::debug("Initiating Bluetooth connection.");

    device->call("Connect", [this](auto result) { finish_connection(result); });
}

void BluetoothSerialPort::pre_connection_scan_progress(
    int percent_complete, const Glib::ustring& device_name) {

    if (m_remote_devices.contains(device_name)) {
        Logger::debug("Device " + device_name + " found.");
        m_pre_connection_scan_result.disconnect();
        initiate_connection(m_remote_devices[device_name]);
    } else if (percent_complete == FINISHED) {
        // Could not find device
        m_pre_connection_scan_result.disconnect();
        m_complete_connection.emit(false);
    }

    // Device not found... just keep probing....
}

bool BluetoothSerialPort::connect(const Glib::ustring& device_name) {
    if (!m_remote_devices.contains(device_name)) {
        // Device not in inventory.  Perform device discovery before attemting
        // to connect.
        Logger::debug("Device " + device_name + " not found in inventory.");
        m_pre_connection_scan_result = m_probe_progress_signal.connect(
            [this, device_name](int percent_complete) {
                pre_connection_scan_progress(percent_complete, device_name);
            });

        probe_remote_devices();
        return true;
    }

    initiate_connection(m_remote_devices[device_name]);
    return true;
}

namespace {
timeval milliseconds_to_time_val(std::chrono::milliseconds time) {
    const std::chrono::seconds seconds =
        std::chrono::duration_cast<std::chrono::seconds>(time);
    time -= seconds;
    return {seconds.count(), std::chrono::microseconds(time).count()};
}
} // namespace

void BluetoothSerialPort::set_timeout(std::chrono::milliseconds timeout) {
    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= 0) {
        const timeval time = milliseconds_to_time_val(timeout);
        setsockopt(m_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
        setsockopt(m_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time));
    }
}

std::shared_ptr<BluetoothSerialPort>
BluetoothSerialPort::get_BluetoothSerialPort() {
    std::shared_ptr<BluetoothSerialPort> bt_ptr =
        m_bluetooth_serial_port.lock();
    if (!bt_ptr) {
        bt_ptr = std::make_shared<BluetoothSerialPort>();
        m_bluetooth_serial_port = bt_ptr;
        // Create BluetoothSerialPort Object Manager
        Gio::DBus::ObjectManagerClient::create_for_bus(
            Gio::DBus::BusType::SYSTEM, "org.bluez", "/",
            [bt_ptr](auto& res) { bt_ptr->manager_created(res); });
    }

    return bt_ptr;
}

void BluetoothSerialPort::manager_created(
    Glib::RefPtr<Gio::AsyncResult>& result) {
    Logger::debug("Entered manager_created.");
    try {
        m_manager =
            Gio::DBus::ObjectManagerClient::create_for_bus_finish(result);
    } catch (Glib::Error& e) {
        Logger::error(e.what());
        return;
    }
    if (m_manager) {
        Logger::debug("Bluetooth manager created.");
        m_object_add_connection = m_manager->signal_object_added().connect(
            [this](auto obj) { add_object(obj); });
        m_object_remove_connection = m_manager->signal_object_removed().connect(
            [this](auto obj) { remove_object(obj); });

        auto objects = m_manager->get_objects();
        for (const auto& object : objects) {
            add_object(object);
        }
    }
    register_profile();
    register_agent();
}

namespace {
Glib::ustring controller_name(const Glib::RefPtr<Gio::DBus::Proxy>& proxy) {
    // controllers will be identified by the object path for now
    return proxy->get_object_path();
}

Glib::ustring device_name(const Glib::RefPtr<Gio::DBus::Proxy>& proxy) {
    // Each device will be identified by its address,
    // since it is guaranteed to be unique.
    Glib::Variant<Glib::ustring> address;
    proxy->get_cached_property(address, "Address");
    return address.get();
}
} // namespace

void BluetoothSerialPort::add_object(
    const Glib::RefPtr<Gio::DBus::Object>& obj) {
    update_object_state(obj, true);
}

void BluetoothSerialPort::remove_object(
    const Glib::RefPtr<Gio::DBus::Object>& obj) {
    update_object_state(obj, false);
}

void BluetoothSerialPort::update_object_state(
    const Glib::RefPtr<Gio::DBus::Object>& obj, bool addObject) {
    enum ObjectType : uint8_t {
        ADAPTER,
        DEVICE,
        AGENT_MANAGER,
        PROFILE_MANAGER
    };

    static std::unordered_map<std::string, ObjectType> objMap = {
        {"org.bluez.Adapter1", ADAPTER},
        {"org.bluez.Device1", DEVICE},
        {"org.bluez.AgentManager1", AGENT_MANAGER},
        {"org.bluez.ProfileManager1", PROFILE_MANAGER}};

    for (auto& interface : obj->get_interfaces()) {
        auto interface_proxy =
            std::dynamic_pointer_cast<Gio::DBus::Proxy>(interface);
        if (!interface_proxy) {
            continue;
        }

        auto interface_name = interface_proxy->get_interface_name();

        if (!objMap.contains(interface_name)) {
            continue;
        }

        auto objectType = objMap[interface_name];

        switch (objectType) {
        case ADAPTER:
        case DEVICE: {
            auto name_func =
                (objectType == ADAPTER) ? controller_name : device_name;
            auto& obj_list =
                (objectType == ADAPTER) ? m_controllers : m_remote_devices;
            if (addObject) {
                obj_list[name_func(interface_proxy)] = interface_proxy;
                Logger::debug("Bluetooth added " + interface_name + ": " +
                              name_func(interface_proxy));
            } else {
                obj_list.erase(name_func(interface_proxy));
                Logger::debug("Bluetooth removed " + interface_name + ": " +
                              name_func(interface_proxy));
            }
            break;
        }
        case AGENT_MANAGER:
        case PROFILE_MANAGER: {
            auto& manager_interface = (objectType == AGENT_MANAGER)
                                          ? m_agent_manager
                                          : m_profile_manager;
            if (addObject) {
                manager_interface = interface_proxy;
                Logger::debug("Bluetooth added " + interface_name);
            } else {
                manager_interface.reset();
                Logger::debug("Bluetooth removed " + interface_name);
            }
            break;
        }
        }
    }
}

bool BluetoothSerialPort::select_controller(
    const Glib::ustring& controller_name) {
    if (m_controllers.contains(controller_name)) {
        m_selected_controller = m_controllers[controller_name];
        return true;
    }

    return false;
}

std::vector<Glib::ustring> BluetoothSerialPort::get_controller_names() {
    std::vector<Glib::ustring> ret;

    ret.reserve(m_controllers.size());
    std::transform(m_controllers.begin(), m_controllers.end(),
                   std::back_inserter(ret),
                   [](const auto& ctlr) { return ctlr.first; });
    return ret;
}

void BluetoothSerialPort::emit_probe_progress(int percent_complete) {
    if (percent_complete == FINISHED) {
        m_probe_in_progress = false;
    }
    m_probe_progress_signal.emit(percent_complete);
}

void BluetoothSerialPort::stop_probe_finish(
    const Glib::RefPtr<Gio::AsyncResult>& result,
    const Glib::RefPtr<Gio::DBus::Proxy>& controller) {
    try {
        controller->call_finish(result);
    } catch (Glib::Error& e) {
        Logger::error(e.what());
    }

    emit_probe_progress(FINISHED);
}

void BluetoothSerialPort::stop_probe() {
    // Timeout occurred; stop probing devices.
    if (m_selected_controller) {
        auto ctlr = m_selected_controller;
        ctlr->call("StopDiscovery", [this, ctlr](auto result) {
            stop_probe_finish(result, ctlr);
        });
    } else {
        emit_probe_progress(FINISHED);
    }
}

bool BluetoothSerialPort::update_probe_progress() {
    emit_probe_progress(m_probe_progress++);
    if (m_probe_progress == FINISHED) {
        stop_probe();
        return false;
    }
    return true;
}

void BluetoothSerialPort::probe_finish(
    Glib::RefPtr<Gio::AsyncResult>& result, std::chrono::seconds timeout,
    const Glib::RefPtr<Gio::DBus::Proxy>& controller) {
    // StartDiscovery command issued; now wait for timeout
    try {
        controller->call_finish(result);
    } catch (Glib::Error& e) {
        Logger::error(e.what());
        emit_probe_progress(FINISHED);
        return;
    }

    // Convert timeout to milliseconds, and interrupt every time
    // we are 100th the way to completion

    static constexpr int TICK_COUNT = 100;
    m_probe_progress = 0;
    m_probe_timer_connection = Glib::signal_timeout().connect(
        [this]() { return update_probe_progress(); },
        static_cast<unsigned int>(std::chrono::milliseconds(timeout).count() /
                                  TICK_COUNT));
}

void BluetoothSerialPort::probe_remote_devices(std::chrono::seconds time) {
    Logger::debug("Probing remote Bluetooth devices.");
    if (m_probe_in_progress) {
        return;
    }

    if (m_selected_controller) {
        m_probe_in_progress = true;
        auto ctlr = m_selected_controller;
        ctlr->call("StartDiscovery", [this, time, ctlr](auto result) {
            probe_finish(result, time, ctlr);
        });
    } else {
        Logger::error("No bluetooth controller selected.");
        emit_probe_progress(FINISHED); // Cannot probe devices
    }
}

sigc::signal<void(int)> BluetoothSerialPort::signal_probe_progress() {
    return m_probe_progress_signal;
}

namespace {
Glib::ustring get_property_value(const Glib::RefPtr<Gio::DBus::Proxy>& proxy,
                                 const Glib::ustring& property_name) {
    Glib::Variant<Glib::ustring> property;

    proxy->get_cached_property(property, property_name);

    return property.get();
}
} // namespace

std::vector<std::pair<Glib::ustring, Glib::ustring>>
BluetoothSerialPort::get_device_names_addresses() {
    std::vector<std::pair<Glib::ustring, Glib::ustring>> ret;

    std::transform(m_remote_devices.begin(), m_remote_devices.end(),
                   std::back_inserter(ret), [](const auto& device) {
                       return std::make_pair<Glib::ustring, Glib::ustring>(
                           device.first,
                           get_property_value(device.second, "Alias"));
                   });

    return ret;
}

constexpr auto OBJECT_PATH = "/com/github/beardedone55/bluetooth_serial";

// Bluetooth profile UUID for Serial Port Profile (SPP)
// See
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/
constexpr auto SERIAL_PORT_UUID = "00001101-0000-1000-8000-00805f9b34fb";

void BluetoothSerialPort::register_complete(
    const Glib::RefPtr<Gio::AsyncResult>& result,
    const Glib::RefPtr<Gio::DBus::Proxy>& registered_manager) {
    // RegisterAgent and RegisterProfile don't
    // return anything.  This just checks if there were
    // errors.
    try {
        registered_manager->call_finish(result);
    } catch (Glib::Error& e) {
        Logger::error(e.what());
    }
    Logger::debug("Bluetooth agent registration complete.");
}

guint BluetoothSerialPort::register_object(
    const std::string& interface_path,
    const Gio::DBus::InterfaceVTable& vtable) {
    if (m_manager) {
        Logger::debug("Acquiring interface definition " + interface_path);

        gsize size = 0;
        auto interface_definition = Glib::ustring(static_cast<const char*>(
            Gio::Resource::lookup_data_global(interface_path)->get_data(size)));

        Logger::debug(interface_definition);

        auto node = Gio::DBus::NodeInfo::create_for_xml(interface_definition);

        Logger::debug("Registering object " + interface_path);

        return m_manager->get_connection()->register_object(
            OBJECT_PATH, node->lookup_interface(), vtable);
    }

    return 0;
}

void BluetoothSerialPort::register_profile() {
    if (!m_profile_manager) {
        return;
    }

    static const Gio::DBus::InterfaceVTable profile_vtable{
        &BluetoothSerialPort::profile_method};

    m_profile_id = register_object("/dbus/bluez-profile1.xml", profile_vtable);

    // RegisterProfile parameters:
    //    String profile
    //    String uuid
    //    Dict   options
    std::vector<Glib::VariantBase> register_profile_params;

    // String profile
    auto profile =
        Glib::Variant<Glib::DBusObjectPathString>::create(OBJECT_PATH);
    register_profile_params.push_back(profile);

    // String uuid
    auto uuid = Glib::Variant<Glib::ustring>::create(SERIAL_PORT_UUID);
    register_profile_params.push_back(uuid);

    // Dict {string, variant} options:
    //    Name = "obd-serial"
    //    Service = SERIAL_PORT_UUID
    //    Role = "client"
    //    Channel = (uint16) 1
    //    AutoConnect = (bool) true
    std::map<Glib::ustring, Glib::VariantBase> options;
    options["Name"] = Glib::Variant<Glib::ustring>::create("obd-serial");
    options["Service"] = uuid;
    options["Role"] = Glib::Variant<Glib::ustring>::create("client");
    options["Channel"] = Glib::Variant<guint16>::create(1);
    options["AutoConnect"] = Glib::Variant<bool>::create(true);

    register_profile_params.push_back(
        Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>>::create(
            options));

    auto parameters =
        Glib::VariantContainerBase::create_tuple(register_profile_params);

    m_profile_manager->call(
        "RegisterProfile",
        [this](auto result) { register_complete(result, m_profile_manager); },
        parameters);

    Logger::debug("Registering bluetooth profile manager.");
}

void BluetoothSerialPort::register_agent() {
    if (!m_agent_manager) {
        return;
    }

    static const Gio::DBus::InterfaceVTable agent_vtable{
        &BluetoothSerialPort::agent_method};

    m_agent_id = register_object("/dbus/bluez-agent1.xml", agent_vtable);

    // RegisterAgent parameters:
    //    String agent
    //    String capabilities
    auto parameters = Glib::VariantContainerBase::create_tuple(
        std::vector<Glib::VariantBase>({
            Glib::Variant<Glib::DBusObjectPathString>::create(OBJECT_PATH),
            Glib::Variant<Glib::ustring>::create("KeyboardDisplay"),
        }));

    m_agent_manager->call(
        "RegisterAgent",
        [this](auto result) { register_complete(result, m_agent_manager); },
        parameters);

    Logger::debug("Registering bluetooth agent manager.");
}

void BluetoothSerialPort::agent_method(
    const Glib::RefPtr<Gio::DBus::Connection>& /*connection*/,
    const Glib::ustring& /*sender*/, const Glib::ustring& /*object_path*/,
    const Glib::ustring& /*interface_name*/, const Glib::ustring& method_name,
    const Glib::VariantContainerBase& parameters,
    const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation) {
    auto bluetooth = m_bluetooth_serial_port.lock();

    if (!bluetooth) {
        return;
    }

    if (method_name == "RequestPinCode") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        bluetooth->request_from_user("Enter Pin Code for " + device_path.get() +
                                         ".",
                                     neon::USER_STRING, invocation);
        return;
    }

    if (method_name == "RequestPasskey") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        bluetooth->request_from_user("Enter Passkey (0-9999999) for " +
                                         device_path.get() + ".",
                                     neon::USER_INT, invocation);
        return;
    }

    if (method_name == "DisplayPinCode") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<Glib::ustring> pin_code;
        parameters.get_child(pin_code, 1);

        bluetooth->request_from_user("Pin Code for " + device_path.get() +
                                         " is " + pin_code.get() + ".",
                                     neon::USER_NONE, {});
        invocation->return_value({});
        return;
    }

    static constexpr int PASSKEY_SIZE = 6;

    if (method_name == "DisplayPasskey") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<guint32> pass_key;
        parameters.get_child(pass_key, 1);

        Glib::Variant<guint16> entered;
        parameters.get_child(entered, 2);

        bluetooth->request_from_user(
            "Passkey for " + device_path.get() + " is " +
                Glib::ustring::format(std::setfill(L'0'),
                                      std::setw(PASSKEY_SIZE), pass_key.get()) +
                ".",
            neon::USER_NONE, {});
        invocation->return_value({});
        return;
    }

    if (method_name == "RequestConfirmation") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);

        Glib::Variant<guint32> pass_key;
        parameters.get_child(pass_key, 1);
        bluetooth->request_from_user(
            "Confirm Passkey for " + device_path.get() + " is " +
                Glib::ustring::format(std::setw(PASSKEY_SIZE), pass_key.get()) +
                ".",
            neon::USER_YN, invocation);

        return;
    }

    if (method_name == "RequestAuthorization") {
        Glib::Variant<Glib::DBusObjectPathString> device_path;
        parameters.get_child(device_path, 0);
        bluetooth->request_from_user("Authorize Connection to Device " +
                                         device_path.get() + "?",
                                     neon::USER_YN, invocation);
        return;
    }

    if (method_name == "AuthorizeService") {
        // I don't know what this is supposed to do, so we'll just
        // return an error for now.
        const Gio::DBus::Error error(Gio::DBus::Error::FAILED,
                                     "org.bluez.Error.Rejected");
        invocation->return_error(error);
    }

    if (method_name == "Cancel") {
        bluetooth->request_from_user("", neon::USER_NONE, {});
        invocation->return_value({});
    }
}

void BluetoothSerialPort::request_from_user(
    const Glib::ustring& message, const ResponseType response_type,
    const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation) {
    // This message emits a signal that should solicit a response from the
    // user. It does the work for "RequestPinCode", "RequestPasskey",
    //"RequestAuthorization", and "RequestConfirmation".

    if (!m_request_user_input.empty()) {

        m_request_user_input.emit(message, response_type, invocation);
        // User is responsible for invoking respond_from_user()
        // to complete operation.
    } else if (invocation) {
        // No one has registered with the signal.
        // Return an error.
        const Gio::DBus::Error error(Gio::DBus::Error::FAILED,
                                     "org.bluez.Error.Rejected");
        invocation->return_error(error);
    }
}

void BluetoothSerialPort::respond_from_user(
    const Glib::VariantBase& response,
    const Glib::RefPtr<void>& signal_handle) {
    const auto invocation =
        std::static_pointer_cast<Gio::DBus::MethodInvocation>(signal_handle);
    if (invocation) {
        if (response && response.is_of_type(Glib::VariantType("b"))) {
            const auto& bool_response =
                Glib::VariantBase::cast_dynamic<const Glib::Variant<bool>>(
                    response);
            if (bool_response && bool_response.get()) {
                invocation->return_value({});
            } else {
                const Gio::DBus::Error error(Gio::DBus::Error::FAILED,
                                             "org.bluez.Error.Rejected");
                invocation->return_error(error);
            }
        } else {
            invocation->return_value(
                Glib::VariantContainerBase::create_tuple(response));
        }
    }
}

void BluetoothSerialPort::profile_method(
    const Glib::RefPtr<Gio::DBus::Connection>& /*connection*/,
    const Glib::ustring& /*sender*/, const Glib::ustring& /*object_path*/,
    const Glib::ustring& /*interface_name*/, const Glib::ustring& method_name,
    const Glib::VariantContainerBase& parameters,
    const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation) {

    Logger::debug("Method " + method_name + " called.");
    auto bluetooth = m_bluetooth_serial_port.lock();

    if (!bluetooth) {
        return;
    }

    if (method_name == "NewConnection") {
        // Parameters:
        //    device
        //    fd
        //    fd_properties

        // 1st parameter is object path
        Glib::Variant<Glib::DBusObjectPathString> obj_path;
        parameters.get_child(obj_path, 0);
        bluetooth->m_connected_device_path = obj_path.get();

        Logger::debug("Parameters = " + parameters.print());

        // 2nd parameter is fd index
        Glib::Variant<gint32> fd_index;
        parameters.get_child(fd_index, 1);
        Logger::debug("fd_index type = " + fd_index.get_type_string());

        // get socket fd from unix fd list
        Logger::debug("New Bluetooh connection requested.");
        auto fd_list = invocation->get_message()->get_unix_fd_list();
        if (bluetooth->m_sock_fd < 0) {
            // Grab the socket, so we can communicate with
            // device, and return to acknowlege connection.
            {
                const std::unique_lock lock(bluetooth->m_sock_fd_mutex);
                bluetooth->m_sock_fd = fd_list->get(fd_index.get());
            }
            Logger::debug("File descriptor for Bluetooth device: " +
                          std::to_string(bluetooth->m_sock_fd));
            // Returns: void
            invocation->return_value({});
        } else // We are already connected to a device (Shouldn't happen...)
        {
            // Close the new socket and return an error.
            Logger::error << "File descriptor was already set to "
                          << bluetooth->m_sock_fd;
            close(fd_list->get(fd_index.get()));
            const Gio::DBus::Error error(Gio::DBus::Error::FAILED,
                                         "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }

        return;
    }

    if (method_name == "RequestDisconnection") {
        // Parameters:
        //   device
        Glib::Variant<Glib::DBusObjectPathString> obj_path;
        parameters.get_child(obj_path, 0);

        if (bluetooth->m_sock_fd >= 0 &&
            obj_path.get() == bluetooth->m_connected_device_path) {
            {
                const std::unique_lock lock(bluetooth->m_sock_fd_mutex);
                close(bluetooth->m_sock_fd);
                bluetooth->m_sock_fd = -1;
            }
            // Returns void
            invocation->return_value({});
        } else // Disconnect requested for unknown device
        {
            // Return an error
            const Gio::DBus::Error error(Gio::DBus::Error::FAILED,
                                         "org.bluez.Error.Rejected");
            invocation->return_error(error);
        }
        return;
    }

    if (method_name == "Release") {
        // Profile was removed by profile manager.
        // Some cleanup could be done, but for now, just ignore.
        // No response is expected.
        return;
    }

    // Unexpected method, return an error (Shouldn't happen)....
    const Gio::DBus::Error error(Gio::DBus::Error::UNKNOWN_METHOD,
                                 "org.bluez.Error.NotSupported");
    invocation->return_error(error);
}
