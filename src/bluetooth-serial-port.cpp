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

#include "bluetooth-serial-port.hpp"
#include "dbus-type.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <sys/socket.h>

// time.h provides timeval
#include <sys/time.h> //NOLINT(misc-include-cleaner)
#include <system_error>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <systemd/sd-bus-protocol.h>
#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using BTSP = BluetoothSerialPort;
using namespace std::literals::chrono_literals;

namespace {
constexpr int FINISHED = 100;
} // namespace

BTSP::BluetoothSerialPort()
    : m_system_bus{get_system_dbus()}, m_event{get_dbus_event()} {

    BTSP::init_event_handler();
    Logger::debug("Created BluetoothSerialPort.");
}

BTSP::~BluetoothSerialPort() {
    if (!m_connected_device_path.empty()) {
        disconnect(nullptr);
    }

    const std::unique_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= -1) {
        close(m_sock_fd);
    }

    Logger::debug << "Destroyed BluetoothSerialPort.\n";
}

BTSP::DBusPtr BTSP::get_system_dbus() {
    sd_bus* system_bus = nullptr;
    if (sd_bus_open_system(&system_bus) < 0) {
        Logger::error("Error connecting to system DBUS.");
    }
    return {system_bus, [](sd_bus* bus) { sd_bus_flush_close_unref(bus); }};
}

BTSP::DBusEventPtr BTSP::get_dbus_event() {
    sd_event* event = nullptr;
    if (!m_system_bus || sd_event_default(&event) < 0) {
        Logger::error("Error getting sd_bus event loop.");
    } else if (sd_bus_attach_event(m_system_bus.get(), event,
                                   SD_EVENT_PRIORITY_NORMAL) < 0) {
        Logger::error("Error attaching event loop to DBUS.");
        event = sd_event_unref(event);
    }
    return {event, [](sd_event* evt) { sd_event_unref(evt); }};
}

bool BTSP::connect_object_manager(sd_bus_message_handler_t callback,
                                  const char* method_name) {

    if (sd_bus_match_signal_async(m_system_bus.get(), nullptr, "org.bluez", "/",
                                  "org.freedesktop.DBus.ObjectManager",
                                  method_name, callback, nullptr, this) < 0) {
        Logger::error << "Error connecting to " << method_name << "\n";
        return false;
    }
    return true;
}

void BTSP::connect_object_manager() {
    if (!m_system_bus ||
        !connect_object_manager(add_object, "InterfacesAdded") ||
        !connect_object_manager(remove_object, "InterfacesRemoved")) {

        Logger::error("Error connecting to object manager.");
    }
}

int BTSP::finish_connection(sd_bus_message* reply, void* userdata,
                            sd_bus_error* /*unused*/) {
    Logger::debug("Connection finished.");
    auto* bt_ptr = static_cast<BTSP*>(userdata);

    if (!bt_ptr->m_complete_connection) {
        Logger::error << "Connection completion handler not registered...\n";
        return 0;
    }

    if (sd_bus_message_is_method_error(reply, nullptr) != 0) {
        Logger::error("Error occurred connecting to Bluetooth Device");
        bt_ptr->m_complete_connection(false);
    } else {
        bt_ptr->m_complete_connection(true);
    }
    bt_ptr->m_complete_connection = nullptr;
    return 0;
}

int BTSP::call_dbus(const std::string& path, const std::string& interface,
                    const std::string& method,
                    sd_bus_message_handler_t callback) {

    sd_bus_message* msg = nullptr;
    int err = sd_bus_message_new_method_call(m_system_bus.get(), &msg,
                                             "org.bluez", path.c_str(),
                                             interface.c_str(), method.c_str());

    if (err < 0) {
        Logger::error << "Call setup for " << method << " failed: "
                      << std::system_error(-err, std::generic_category()).what()
                      << "(" << -err << ")\n";
        return err;
    }

    if (callback != nullptr) {
        Logger::debug << "Calling async method " << method << "\n";
        err = sd_bus_call_async(m_system_bus.get(), nullptr, msg, callback,
                                this, 0);
    } else {
        Logger::debug << "Calling sync method " << method << "\n";
        err = sd_bus_call(m_system_bus.get(), msg, 0, nullptr, nullptr);
    }

    if (err < 0) {
        Logger::error << "Call to " << method << " failed: "
                      << std::system_error(-err, std::generic_category()).what()
                      << "(" << -err << ")\n";
    }

    sd_bus_message_unref(msg);

    return err;
}

void BTSP::initiate_connection(const std::string& device_address) {
    Logger::debug("Initiating Bluetooth connection.");
    Logger::debug << "Device " << device_address << " : "
                  << m_dev_name_path_map[device_address] << "\n";
    call_dbus(m_dev_name_path_map[device_address], "org.bluez.Device1",
              "Connect", finish_connection);
}

int BTSP::finish_disconnect(sd_bus_message* /*unused*/,
                            void* userdata /*unused*/,
                            sd_bus_error* /*unused*/) {
    Logger::debug("Disconnect finished.");
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    if (bt_ptr->m_complete_disconnect) {
        bt_ptr->m_complete_disconnect();
    }
    bt_ptr->m_complete_disconnect = nullptr;
    return 0;
}

void BTSP::initiate_disconnect(const std::string& device_path) {
    Logger::debug("Initiating Bluetooth disconnect.");

    sd_bus_message_handler_t callback =
        (m_complete_disconnect) ? finish_disconnect : nullptr;
    call_dbus(device_path, "org.bluez.Device1", "Disconnect", callback);
}

void BTSP::disconnect(std::function<void()> callback) {
    if (m_complete_disconnect) {
        Logger::error << "Disconnect called but already in progress.";
        return;
    }
    if (m_connected_device_path.empty()) {
        Logger::error << "Disconnect called but no device connected.";
        return;
    }
    m_complete_disconnect = std::move(callback);
    initiate_disconnect(m_connected_device_path);
}

void BTSP::pre_connection_scan_progress(int percent_complete,
                                        const std::string& device_address) {

    if (m_dev_name_path_map.contains(device_address)) {
        Logger::debug("Device " + device_address + " found.");
        initiate_connection(device_address);
        m_probe_callback = nullptr;
    } else if (percent_complete == FINISHED) {
        // Could not find device
        m_probe_callback = nullptr;
        if (m_complete_connection) {
            m_complete_connection(false);
            m_complete_connection = nullptr;
        }
    }

    // Device not found... just keep probing....
}

bool BTSP::connect(const std::string& device_address,
                   std::function<void(bool)> callback) {
    if (m_complete_connection) {
        Logger::error << "Connection already in progress...\n";
        return false;
    }

    m_complete_connection = std::move(callback);

    if (!m_dev_name_path_map.contains(device_address)) {
        // Device not in inventory.  Perform device discovery before attemting
        // to connect.
        Logger::debug("Device " + device_address + " not found in inventory.");
        probe_remote_devices([this, device_address](int percent_complete) {
            pre_connection_scan_progress(percent_complete, device_address);
        });

        return true;
    }

    initiate_connection(device_address);
    return true;
}

// timeval is provided by sys/time.h
// NOLINTNEXTLINE(misc-include-cleaner)
timeval BTSP::milliseconds_to_time_val(std::chrono::milliseconds time) {
    const std::chrono::seconds seconds =
        std::chrono::duration_cast<std::chrono::seconds>(time);
    time -= seconds;
    return {.tv_sec = seconds.count(),
            .tv_usec = std::chrono::microseconds(time).count()};
}

void BTSP::set_timeout(std::chrono::milliseconds timeout) {
    const std::shared_lock lock(m_sock_fd_mutex);
    if (m_sock_fd >= 0) {
        // timeval is provided by sys/time.h
        // NOLINTNEXTLINE(misc-include-cleaner)
        const timeval time = milliseconds_to_time_val(timeout);
        // SOL_SOCKET, SO_RCVTIMEO, and SO_SNDTIMEO are provided
        // by sys/socket.h
        // NOLINTBEGIN(misc-include-cleaner)
        setsockopt(m_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
        setsockopt(m_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time));
        // NOLINTEND(misc-include-cleaner)
    }
}

void BTSP::get_objects() {
    Logger::debug << "get_objects() called.\n";
    call_dbus("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
              get_objects_complete);
}

int BTSP::get_objects_complete(sd_bus_message* reply, void* userdata,
                               sd_bus_error* /*unused*/) {
    Logger::debug << "get_objects_complete() called.\n";
    if (sd_bus_message_is_method_error(reply, nullptr) != 0) {
        Logger::error << "Failed to get DBus Objects from Object Manager.\n";
        return 0;
    }
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    const DBusType msg(*reply);
    bt_ptr->connect_object_manager();
    for (auto& [path, obj] : msg.getDict()) {
        bt_ptr->add_object(std::get<std::string>(path), obj);
    }
    bt_ptr->register_profile();
    bt_ptr->register_agent();
    return 0;
}

int BTSP::add_object(sd_bus_message* msg, void* userdata,
                     sd_bus_error* /*unused*/) {
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    const std::string path = DBusType(*msg).getString();
    const DBusType obj(*msg);
    return bt_ptr->add_object(path, obj);
}

int BTSP::remove_object(sd_bus_message* msg, void* userdata,
                        sd_bus_error* /*unused*/) {
    BTSP* bt_ptr = static_cast<BTSP*>(userdata);
    const std::string path = DBusType(*msg).getString();
    const DBusType obj(*msg);
    return bt_ptr->remove_object(path, obj);
}

int BTSP::add_object(const std::string& path, const DBusType& obj) {
    Logger::debug << "Added " << path << ".\n";
    for (const auto& [interface, properties] : obj.getDict()) {

        const auto interface_name = std::get<std::string>(interface);

        if (interface_name == "org.bluez.Adapter1") {
            if (m_controllers.contains(path)) {
                m_controllers.at(path) = properties;
            } else {
                m_controllers.emplace(path, properties);
            }
        } else if (interface_name == "org.bluez.Device1") {
            if (m_remote_devices.contains(path)) {
                m_remote_devices.at(path) = properties;
            } else {
                m_remote_devices.emplace(path, properties);
            }
            m_dev_name_path_map[properties.at("Address").getString()] = path;
            Logger::debug << "Added Device "
                          << properties.at("Alias").getString() << " : "
                          << properties.at("Address").getString() << "\n";
        } else if (interface_name == "org.bluez.AgentManager1") {
            m_agent_manager = path;
        } else if (interface_name == "org.bluez.ProfileManager1") {
            m_profile_manager = path;
        }
    }
    return 0;
}

int BTSP::remove_object(const std::string& path, const DBusType& obj) {
    Logger::debug << "Removed " << path << ".\n";
    for (const auto& interface : obj.getArray()) {

        auto interface_name = interface.getString();

        if (interface_name == "org.bluez.Adapter1") {
            m_controllers.erase(path);
        } else if (interface_name == "org.bluez.Device1") {
            if (m_remote_devices.contains(path)) {
                m_dev_name_path_map.erase(
                    m_remote_devices.at(path).at("Address").getString());
                m_remote_devices.erase(path);
            }
        } else if (interface_name == "org.bluez.AgentManager1") {
            m_agent_manager = "";
        } else if (interface_name == "org.bluez.ProfileManager1") {
            m_profile_manager = "";
        }
    }
    return 0;
}

void BTSP::init_event_handler() {
    BTSP::process_events();
    get_objects();
}

void BTSP::process_events() {
    if (m_event) {
        while (sd_event_run(m_event.get(), 0) > 0) {
        };
    }
}

int BTSP::get_event_fd() const {
    Logger::debug << "get_event_fd: m_event = " << m_event.get() << "\n";
    const int result = sd_event_get_fd(m_event.get());
    Logger::debug << "Event fd = " << result << "\n";
    return result;
}

bool BTSP::select_controller(const std::string& controller_name) {
    if (m_controllers.contains(controller_name)) {
        m_selected_controller = controller_name;
        return true;
    }

    return false;
}

std::vector<std::string> BTSP::get_controller_names() {
    std::vector<std::string> ret;

    ret.reserve(m_controllers.size());
    for (const auto& [name, address] : m_controllers) {
        ret.emplace_back(name);
    }
    return ret;
}

void BTSP::emit_probe_progress(int percent_complete) {
    if (percent_complete == FINISHED) {
        m_probe_in_progress = false;
    }
    if (m_probe_callback) {
        m_probe_callback(percent_complete);
    }
}

int BTSP::stop_probe_finish(sd_bus_message* reply, void* userdata,
                            sd_bus_error* /*unused*/) {
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    if (sd_bus_message_is_method_error(reply, nullptr) != 0) {
        Logger::error << "StopDiscovery completed in error\n";
    }

    bt_ptr->emit_probe_progress(FINISHED);
    return 0;
}

void BTSP::stop_probe() {
    // Timeout occurred; stop probing devices.
    if (!m_selected_controller.empty()) {
        auto ctlr = m_selected_controller;
        const int err = call_dbus(ctlr, "org.bluez.Adapter1", "StopDiscovery",
                                  stop_probe_finish);

        if (err < 0) {
            emit_probe_progress(FINISHED); // Cannot probe devices
        }
    } else {
        emit_probe_progress(FINISHED);
    }
}

std::uint64_t BTSP::get_tick_time(std::chrono::seconds probe_time) {
    static constexpr int TICK_COUNT = 100;
    return static_cast<std::uint64_t>(
        std::chrono::microseconds(probe_time).count() / TICK_COUNT);
}

int BTSP::update_probe_progress(sd_event_source* evt_src,
                                std::uint64_t /*unused*/, void* userdata) {
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    bt_ptr->emit_probe_progress(bt_ptr->m_probe_progress++);
    if (bt_ptr->m_probe_progress == FINISHED) {
        sd_event_source_set_enabled(evt_src, SD_EVENT_OFF);
        sd_event_source_unref(evt_src);
        bt_ptr->stop_probe();
    } else {
        sd_event_source_set_time_relative(evt_src,
                                          get_tick_time(bt_ptr->m_probe_time));
    }
    return 0;
}

int BTSP::probe_finish(sd_bus_message* reply, void* userdata,
                       sd_bus_error* /*unused*/) {
    // StartDiscovery command issued; now wait for timeout

    auto* bt_ptr = static_cast<BTSP*>(userdata);

    if (sd_bus_message_is_method_error(reply, nullptr) != 0) {
        Logger::error << "DeviceDiscovery completed in error\n";
        bt_ptr->emit_probe_progress(FINISHED);
        return 0;
    }

    // Convert timeout to milliseconds, and interrupt every time
    // we are 100th the way to completion

    bt_ptr->m_probe_progress = 0;

    sd_event_source* evt_src = nullptr;

    // CLOCK_MONOTONIC is provided by sys/time.h
    sd_event_add_time_relative(sd_bus_get_event(bt_ptr->m_system_bus.get()),
                               &evt_src,
                               CLOCK_MONOTONIC, // NOLINT(misc-include-cleaner)
                               get_tick_time(bt_ptr->m_probe_time), 0,
                               update_probe_progress, userdata);
    sd_event_source_set_enabled(evt_src, SD_EVENT_ON);
    return 0;
}

void BTSP::probe_remote_devices(std::function<void(int)> callback,
                                std::chrono::seconds time) {
    Logger::debug("Probing remote Bluetooth devices.");
    if (m_probe_in_progress) {
        return;
    }

    if (!m_selected_controller.empty()) {
        m_probe_in_progress = true;
        m_probe_time = time;
        m_probe_callback = std::move(callback);
        auto ctlr = m_selected_controller;
        const int err = call_dbus(ctlr, "org.bluez.Adapter1", "StartDiscovery",
                                  probe_finish);

        if (err < 0) {
            emit_probe_progress(FINISHED); // Cannot probe devices
        }
    } else {
        Logger::error("No bluetooth controller selected.");
        emit_probe_progress(FINISHED); // Cannot probe devices
    }
}

std::vector<BTSP::DeviceInfo> BTSP::get_device_names_addresses() {
    std::vector<DeviceInfo> ret;

    for (const auto& [path, properties] : m_remote_devices) {
        ret.emplace_back(properties.at("Alias").getString(),
                         properties.at("Address").getString());
    }

    return ret;
}

constexpr auto OBJECT_PATH = "/com/github/beardedone55/bluetooth_serial";

// Bluetooth profile UUID for Serial Port Profile (SPP)
// See
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/
constexpr auto SERIAL_PORT_UUID = "00001101-0000-1000-8000-00805f9b34fb";

int BTSP::register_complete(sd_bus_message* msg, void* /*unused*/,
                            sd_bus_error* /*unused*/) {
    if (sd_bus_message_is_method_error(msg, nullptr) != 0) {
        Logger::error << "Error occurred registering agent.\n";
    }
    Logger::debug("Bluetooth agent registration complete.");
    return 0;
}

int BTSP::register_object(const std::string& interface,
                          const std::span<const sd_bus_vtable>& vtable) {
    if (m_system_bus) {
        Logger::debug << "Registering object " << interface << "\n";

        return sd_bus_add_object_vtable(m_system_bus.get(), nullptr,
                                        OBJECT_PATH, interface.c_str(),
                                        vtable.data(), this);
    }

    return 0;
}

int BTSP::bt_release(sd_bus_message* /*unused*/, void* /*unused*/,
                     sd_bus_error* /*unused*/) {
    // Profile was removed by profile manager.
    // Some cleanup could be done, but for now, just ignore.
    // No response is expected.
    return 0;
}

void BTSP::dbus_return_void(sd_bus_message* msg) {
    sd_bus_message* reply = nullptr;
    const int err = sd_bus_message_new_method_return(msg, &reply);
    if (err >= 0) {
        sd_bus_message_send(reply);
    }
    sd_bus_message_unref(reply);
}

void BTSP::dbus_return_error(sd_bus_message* msg, const std::string& error,
                             const std::string& error_string) {

    sd_bus_error err{};
    if (sd_bus_error_set(&err, error.c_str(), error_string.c_str()) >= 0) {
        sd_bus_reply_method_error(msg, &err);
    }
}

int BTSP::bt_new_connection(sd_bus_message* msg, void* userdata,
                            sd_bus_error* /*unused*/) {
    const std::string obj_path = DBusType(*msg).getString();
    const auto sock_fd = DBusType(*msg).getValue<std::int32_t>();
    auto* bt_ptr = static_cast<BTSP*>(userdata);

    Logger::debug("New Bluetooth connection requested.");
    if (bt_ptr->m_sock_fd < 0) {
        bt_ptr->m_connected_device_path = obj_path;
        // Grab the socket, so we can communicate with
        // device, and return to acknowlege connection.
        {
            const std::unique_lock lock(bt_ptr->m_sock_fd_mutex);
            bt_ptr->m_sock_fd = dup(sock_fd);
        }
        Logger::debug("File descriptor for Bluetooth device: " +
                      std::to_string(bt_ptr->m_sock_fd));
        // Returns: void
        dbus_return_void(msg);
    } else { // We are already connected to a device (Shouldn't happen...)
        // Close the new socket and return an error.
        Logger::error << "File descriptor was already set to "
                      << bt_ptr->m_sock_fd;
        dbus_return_error(msg, "org.bluez.Error.Rejected",
                          "Already connected to a bluetooth device.");
    }

    return 0;
}

int BTSP::bt_request_disconnection(sd_bus_message* msg, void* userdata,
                                   sd_bus_error* /*unused*/) {
    const std::string obj_path = DBusType(*msg).getString();
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    if (bt_ptr->m_sock_fd >= 0 && obj_path == bt_ptr->m_connected_device_path) {
        {
            const std::unique_lock lock(bt_ptr->m_sock_fd_mutex);
            close(bt_ptr->m_sock_fd);
            bt_ptr->m_sock_fd = -1;
        }
        // Returns void
        dbus_return_void(msg);
    } else { // Disconnect requested for unknown device
        // Return an error
        dbus_return_error(
            msg, "org.bluez.Error.Rejected",
            "Unexpected device requested disconnection: " + obj_path + ".");
    }
    bt_ptr->m_connected_device_path = "";
    return 0;
}

const std::array<sd_bus_vtable, 5> BTSP::profile_vtable{
    {SD_BUS_VTABLE_START(0),
     SD_BUS_METHOD_WITH_ARGS("Release", SD_BUS_NO_ARGS, SD_BUS_NO_RESULT,
                             BTSP::bt_release, 0),
     SD_BUS_METHOD_WITH_ARGS(
         "NewConnection",
         SD_BUS_ARGS("o", device, "h", fd, "a{sv}", fd_properties),
         SD_BUS_NO_RESULT, BTSP::bt_new_connection, 0),
     SD_BUS_METHOD_WITH_ARGS("RequestDisconnection", SD_BUS_ARGS("o", device),
                             SD_BUS_NO_RESULT, BTSP::bt_request_disconnection,
                             0),
     SD_BUS_VTABLE_END}};

namespace {
struct DBusVariantVisitor {
    explicit DBusVariantVisitor(sd_bus_message* msg) : m_msg{msg} {}
    int operator()(int val) const {
        int res = sd_bus_message_open_container(m_msg, 'v', "q");
        if (res < 0) {
            return res;
        }
        auto intval = static_cast<std::uint16_t>(val);
        res = sd_bus_message_append_basic(m_msg, 'q', &intval);
        if (res < 0) {
            return res;
        }
        return sd_bus_message_close_container(m_msg);
    }

    int operator()(const std::string& val) const {
        int res = sd_bus_message_open_container(m_msg, 'v', "s");
        if (res < 0) {
            return res;
        }
        res = sd_bus_message_append_basic(m_msg, 's', val.c_str());
        if (res < 0) {
            return res;
        }
        return sd_bus_message_close_container(m_msg);
    }

    int operator()(bool val) const {
        int res = sd_bus_message_open_container(m_msg, 'v', "b");
        if (res < 0) {
            return res;
        }
        auto boolval = static_cast<int>(val);
        res = sd_bus_message_append_basic(m_msg, 'b', &boolval);
        if (res < 0) {
            return res;
        }
        return sd_bus_message_close_container(m_msg);
    }

  private:
    sd_bus_message* m_msg = nullptr;
};
} // namespace

int BTSP::dbus_message_append_dict(sd_bus_message* msg, DBusDict dict) {
    int res = sd_bus_message_open_container(msg, 'a', "{sv}");
    if (res >= 0) {
        for (const auto& [key, val] : dict) {
            res = sd_bus_message_open_container(msg, 'e', "sv");
            if (res < 0) {
                return res;
            }
            res = sd_bus_message_append_basic(msg, 's', key.c_str());
            if (res < 0) {
                return res;
            }
            res = std::visit(DBusVariantVisitor{msg}, val);
            if (res < 0) {
                return res;
            }
            res = sd_bus_message_close_container(msg);
            if (res < 0) {
                return res;
            }
        }
        res = sd_bus_message_close_container(msg);
    }
    return res;
}

void BTSP::register_profile() {
    if (m_profile_manager.empty()) {
        return;
    }

    if (register_object("org.bluez.Profile1", profile_vtable) < 0) {
        Logger::error << "Error occurred registering profile with DBus\n";
        return;
    }

    sd_bus_message* msg = nullptr;

    static const std::array<
        std::pair<std::string, std::variant<std::string, int, bool>>, 5>
        params = {{{"Name", "obd-serial"},
                   {"Service", SERIAL_PORT_UUID},
                   {"Role", "client"},
                   {"Channel", 1},
                   {"AutoConnect", true}}};

    if (sd_bus_message_new_method_call(
            m_system_bus.get(), &msg, "org.bluez", m_agent_manager.c_str(),
            "org.bluez.ProfileManager1", "RegisterProfile") >= 0 &&
        sd_bus_message_append_basic(msg, 'o', OBJECT_PATH) >= 0 &&
        sd_bus_message_append_basic(msg, 's', SERIAL_PORT_UUID) >= 0 &&
        dbus_message_append_dict(msg, params) >= 0) {

        sd_bus_call_async(m_system_bus.get(), nullptr, msg, register_complete,
                          nullptr, 0);

    } else {
        Logger::error << "Failed to register profile with bluez.\n";
    }

    Logger::debug("Registering bluetooth profile manager.");
}

int BTSP::bt_agent_request(sd_bus_message* msg, void* userdata,
                           const std::string& text,
                           const ResponseType response_type) {
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    auto device_path = DBusType(*msg).getString();
    std::string device_name;
    if (bt_ptr->m_remote_devices.contains(device_path)) {
        device_name =
            bt_ptr->m_remote_devices.at(device_path).at("Alias").getString();
    } else {
        device_name = device_path;
    }
    std::string punctuation;
    if (response_type == neon::USER_YN) {
        punctuation = "?";
    } else {
        punctuation = ".";
    }
    bt_ptr->request_from_user(text + device_name + punctuation, response_type,
                              msg);
    return 1;
}

std::string BTSP::format_passkey(std::uint32_t value) {
    std::stringstream stream;
    static constexpr int PASSKEY_SIZE = 6;
    stream << std::setfill('0') << std::setw(PASSKEY_SIZE) << value;
    return stream.str();
}

template <typename T>
int BTSP::bt_agent_display(sd_bus_message* msg, void* userdata,
                           const std::string& text) {
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    const std::string device_path = DBusType(*msg).getString();
    std::string value_str;
    if constexpr (std::is_integral_v<T>) {
        T value = DBusType(*msg).getValue<T>();
        value_str = format_passkey(value);
    } else {
        value_str = DBusType(*msg).getString();
    }
    bt_ptr->request_from_user(text + device_path + " is " + value_str + ".",
                              neon::USER_NONE, nullptr);
    dbus_return_void(msg);
    return 0;
}

int BTSP::bt_agent_release(sd_bus_message* /*unused*/, void* /*unused*/,
                           sd_bus_error* /*unused*/) {
    return 0;
}

int BTSP::bt_request_pin_code(sd_bus_message* msg, void* userdata,
                              sd_bus_error* /*unused*/) {
    return bt_agent_request(msg, userdata, "Enter Pin Code for ");
}

int BTSP::bt_display_pin_code(sd_bus_message* msg, void* userdata,
                              sd_bus_error* /*unused*/) {
    return bt_agent_display<std::string>(msg, userdata, "Pin Code for ");
}

int BTSP::bt_request_passkey(sd_bus_message* msg, void* userdata,
                             sd_bus_error* /*unused*/) {
    return bt_agent_request(msg, userdata, "Enter Passkey (0-9999999) for ",
                            neon::USER_INT);
}

int BTSP::bt_display_passkey(sd_bus_message* msg, void* userdata,
                             sd_bus_error* /*unused*/) {
    return bt_agent_display<std::uint32_t>(msg, userdata, "Passkey for ");
}

int BTSP::bt_request_confirmation(sd_bus_message* msg, void* userdata,
                                  sd_bus_error* /*unused*/) {
    auto* bt_ptr = static_cast<BTSP*>(userdata);
    const std::string device_path = DBusType(*msg).getString();
    auto passkey = DBusType(*msg).getValue<std::uint32_t>();
    const std::string passkey_str = format_passkey(passkey);
    bt_ptr->request_from_user("Confirm Passkey for " + device_path + " is " +
                                  passkey_str + ".",
                              neon::USER_YN, msg);

    return 1;
}

int BTSP::bt_request_authorization(sd_bus_message* msg, void* userdata,
                                   sd_bus_error* /*unused*/) {
    return bt_agent_request(msg, userdata, "Authorize Connection to Device ",
                            neon::USER_YN);
}

int BTSP::bt_authorize_service(sd_bus_message* msg, void* /*unused*/,
                               sd_bus_error* /*unused*/) {
    // I don't know what this is supposed to do, so we'll just
    // return an error for now.
    dbus_return_error(msg, "org.bluez.Error.Rejected",
                      "Unknown Method AuthorizeService");
    return 0;
}

int BTSP::bt_agent_cancel(sd_bus_message* msg, void* /*unused*/,
                          sd_bus_error* /*unused*/) {
    dbus_return_void(msg);
    return 0;
}

const std::array<sd_bus_vtable, 11> BTSP::agent_vtable{
    {SD_BUS_VTABLE_START(0),
     SD_BUS_METHOD_WITH_ARGS("Release", SD_BUS_NO_ARGS, SD_BUS_NO_RESULT,
                             BTSP::bt_agent_release, 0),
     SD_BUS_METHOD_WITH_ARGS("RequestPinCode", SD_BUS_ARGS("o", device),
                             SD_BUS_RESULT("s", pincode),
                             BTSP::bt_request_pin_code, 0),
     SD_BUS_METHOD_WITH_ARGS("DisplayPinCode",
                             SD_BUS_ARGS("o", device, "s", pincode),
                             SD_BUS_NO_RESULT, BTSP::bt_display_pin_code, 0),
     SD_BUS_METHOD_WITH_ARGS("RequestPasskey", SD_BUS_ARGS("o", device),
                             SD_BUS_RESULT("u", passkey),
                             BTSP::bt_request_passkey, 0),
     SD_BUS_METHOD_WITH_ARGS(
         "DisplayPasskey", SD_BUS_ARGS("o", device, "u", passkey, "q", entered),
         SD_BUS_NO_RESULT, BTSP::bt_display_passkey, 0),
     SD_BUS_METHOD_WITH_ARGS(
         "RequestConfirmation", SD_BUS_ARGS("o", device, "u", passkey),
         SD_BUS_NO_RESULT, BTSP::bt_request_confirmation, 0),
     SD_BUS_METHOD_WITH_ARGS("RequestAuthorization", SD_BUS_ARGS("o", device),
                             SD_BUS_NO_RESULT, BTSP::bt_request_authorization,
                             0),
     SD_BUS_METHOD_WITH_ARGS("AuthorizeService",
                             SD_BUS_ARGS("o", device, "s", uuid),
                             SD_BUS_NO_RESULT, BTSP::bt_authorize_service, 0),
     SD_BUS_METHOD_WITH_ARGS("Cancel", SD_BUS_NO_ARGS, SD_BUS_NO_RESULT,
                             BTSP::bt_agent_cancel, 0),
     SD_BUS_VTABLE_END}};

void BTSP::register_agent() {
    if (m_agent_manager.empty()) {
        return;
    }

    if (register_object("org.bluez.Agent1", agent_vtable) < 0) {
        Logger::error << "Error occurred registering agent with DBus.\n";
        return;
    }

    sd_bus_message* msg = nullptr;
    if (sd_bus_message_new_method_call(
            m_system_bus.get(), &msg, "org.bluez", m_agent_manager.c_str(),
            "org.bluez.AgentManager1", "RegisterAgent") >= 0 &&
        sd_bus_message_append_basic(msg, 'o', OBJECT_PATH) >= 0 &&
        sd_bus_message_append_basic(msg, 's', "KeyboardDisplay") >= 0) {

        sd_bus_call_async(m_system_bus.get(), nullptr, msg, register_complete,
                          nullptr, 0);
    }

    Logger::debug("Registering bluetooth agent manager.");
}

void BTSP::request_from_user(const std::string& message,
                             const ResponseType response_type,
                             sd_bus_message* msg) {
    // This message emits a signal that should solicit a response from the
    // user. It does the work for "RequestPinCode", "RequestPasskey",
    //"RequestAuthorization", and "RequestConfirmation".

    if (m_request_user_input) {
        if (msg != nullptr) {
            sd_bus_message_ref(msg);
        }
        m_request_user_input(message, response_type, msg);
        // User is responsible for invoking respond_from_user()
        // to complete operation.
    } else if (msg != nullptr) {
        // No one has registered with the signal.
        // Return an error.
        dbus_return_error(msg, "org.bluez.Error.Rejected",
                          "No handler for method.");
    }
}

void BTSP::dbus_return_string(sd_bus_message* msg, const std::string& str) {
    sd_bus_message* reply = nullptr;
    const int err = sd_bus_message_new_method_return(msg, &reply);
    if (err >= 0) {
        if (sd_bus_message_append_basic(reply, 's', str.c_str()) >= 0) {
            sd_bus_message_send(reply);
        }
    }
    sd_bus_message_unref(reply);
}

void BTSP::dbus_return_int(sd_bus_message* msg, int val) {
    sd_bus_message* reply = nullptr;
    const int err = sd_bus_message_new_method_return(msg, &reply);
    if (err >= 0) {
        if (sd_bus_message_append_basic(reply, 'u', &val) >= 0) {
            sd_bus_message_send(reply);
        }
    }
    sd_bus_message_unref(reply);
}

void BTSP::respond_from_user(const ResponseVariant& response, void* handle) {
    auto* msg = static_cast<sd_bus_message*>(handle);
    if (msg != nullptr) {
        if (std::holds_alternative<bool>(response)) {
            const bool bool_response = std::get<bool>(response);
            if (bool_response) {
                dbus_return_void(msg);
            } else {
                dbus_return_error(msg, "org.bluez.Error.Rejected",
                                  "No response from user.");
            }
        } else {
            if (std::holds_alternative<std::string>(response)) {
                dbus_return_string(msg, std::get<std::string>(response));
            } else {
                dbus_return_int(msg, std::get<int>(response));
            }
        }
        sd_bus_message_unref(msg);
    }
}
