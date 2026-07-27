/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2026 Brian LePage
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

/*  Unit tests for functionality of DBusType class.
 *  Requires that bluez is installed and running.
 */

#include "dbus-type.hpp"
#include "logger.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <systemd/sd-bus.h>
#include <utility>
#include <variant>
#include <vector>

// NOLINTBEGIN(misc-use-anonymous-namespace)

static void print_err(const std::string& msg, int err) {
    Logger::error << msg << ": "
                  << std::error_code(err, std::generic_category()).message()
                  << "(" << err << ")\n";
}

static constexpr std::string_view expected_signature = "a{oa{sa{sv}}}";

static bool check_response_signature(const std::string& sig) {
    return sig == expected_signature;
}

static void process_array(const DBusType& type, std::stringstream& stream) {
    std::vector<DBusType> arr;
    try {
        arr = type.getArray();
    } catch (const std::bad_variant_access& e) {
        stream << "[]\n";
        return;
    }

    stream << "[\n                         ";

    for (const auto& val : arr) {
        if (val.getType() == "s") {
            stream << val.getString();
        } else {
            stream << val.getNumber();
        }
        stream << "\n                         ";
    }

    stream << "\b\b]\n";
}

static void stringify_property(const DBusType& property_value,
                               std::stringstream& stream) {
    switch (property_value.getType().at(0)) {
    case 'b':
        stream << std::boolalpha << property_value.getValue<bool>() << "\n";
        break;
    case 'y':
        stream << static_cast<int>(property_value.getValue<uint8_t>()) << "\n";
        break;
    case 'n':
        stream << property_value.getValue<int16_t>() << "\n";
        break;
    case 'q':
        stream << property_value.getValue<uint16_t>() << "\n";
        break;
    case 'i':
    case 'h':
        stream << property_value.getValue<int32_t>() << "\n";
        break;
    case 'u':
        stream << property_value.getValue<uint32_t>() << "\n";
        break;
    case 'x':
        stream << property_value.getValue<int64_t>() << "\n";
        break;
    case 't':
        stream << property_value.getValue<uint64_t>() << "\n";
        break;
    case 'd':
        stream << property_value.getValue<double>() << "\n";
        break;
    case 's':
    case 'o':
        stream << property_value.getValue<std::string>() << "\n";
        break;
    case 'a':
        process_array(property_value, stream);
        break;
    default:
        stream << "[]\n";
    }
}

static void show_device_info(const DBusType& obj) {
    Logger::debug << "This is a bluetooth device!\n";
    Logger::debug
        << "Its device address is "
        << obj.at("org.bluez.Device1").at("Address").getValue<std::string>()
        << "\n";
    Logger::debug << "Its first UUID is "
                  << obj.at("org.bluez.Device1").at("UUIDs").at(0U).getString()
                  << "\n";
}

static bool boolean_test(const DBusType& obj) {
    auto paired_bool = obj.at("org.bluez.Device1").at("Paired").getBool();
    auto paired_number = obj.at("org.bluez.Device1").at("Paired").getNumber();

    Logger::debug << "Testing getNumber() on boolean type.\n";
    if (paired_bool != static_cast<bool>(paired_number)) {
        Logger::error << "getNumber() returned wrong value.  Expected: "
                      << paired_bool << " Received: " << paired_number << "\n";
        return false;
    }

    Logger::debug << "  Paired =  " << std::boolalpha << paired_bool << "\n";
    Logger::debug << "  Paired =  " << std::boolalpha << paired_number << "\n";

    Logger::debug << "Testing getBasicType() on boolean type.\n";
    auto paired_basic_type =
        obj.at("org.bluez.Device1").at("Paired").getBasicType();

    if (paired_bool != std::get<bool>(paired_basic_type)) {
        Logger::error << "getBasicType() returned wrong value.  Expected: "
                      << paired_bool
                      << " Received: " << std::get<bool>(paired_basic_type)
                      << "\n";
        return false;
    }
    return true;
}

static bool get_basic_type_error_test(const DBusType& obj) {
    Logger::debug << "Testing getBasicType() on container type.\n";
    try {
        obj.at("org.bluez.Device1").getBasicType();
        Logger::error << " getBasicType() should have thrown exception "
                         "for container type.\n";
        return false;
    } catch (const std::bad_variant_access& e) {
        Logger::debug << " Device interface is not a basic type.\n";
    }
    return true;
}

static bool get_string_error_test(const DBusType& obj) {
    Logger::debug << "Testing getString() on boolean type.\n";
    try {
        obj.at("org.bluez.Device1").at("Paired").getString();
        Logger::error << " getString() should have thrown exception "
                         "for Paired property\n";
        return false;
    } catch (const std::bad_variant_access& e) {
        Logger::debug << " Paired property is not a string.\n";
    }
    return true;
}

static bool get_number_error_test(const DBusType& obj) {
    Logger::debug << "Testing getNumber() on string type.\n";
    try {
        obj.at("org.bluez.Device1").at("Address").getNumber();
        Logger::error << " getNumber() should have thrown exception "
                         "for Address property\n";
        return false;
    } catch (const std::bad_variant_access& e) {
        Logger::debug << " Address property is not a number.\n";
    }
    return true;
}

static bool get_uint32_error_test(const DBusType& obj) {
    Logger::debug << "Testing getValue<std::uint32_t>() on boolean type.\n";
    try {
        obj.at("org.bluez.Device1").at("Paired").getValue<std::uint32_t>();
        Logger::error << " getValue<std::uint32_t>() should have "
                         "thrown exception for Paired property\n";
        return false;
    } catch (const std::bad_variant_access& e) {
        Logger::debug << " Paired property is a boolean, not a uint32_t.\n";
    }
    return true;
}

static bool run_error_tests(const DBusType& obj) {
    return boolean_test(obj) && get_basic_type_error_test(obj) &&
           get_string_error_test(obj) && get_number_error_test(obj) &&
           get_uint32_error_test(obj);
}

static bool get_number_test(const DBusType& obj) {
    if (obj.at("org.bluez.Device1").getDict().contains("Class")) {
        Logger::debug << "Testing getNumber() and getValue<std::uint32_t>() on "
                         "uint32_t type.\n";
        auto class_value1 = obj.at("org.bluez.Device1").at("Class").getNumber();
        auto class_value2 =
            obj.at("org.bluez.Device1").at("Class").getValue<std::uint32_t>();
        if (std::cmp_not_equal(class_value1, class_value2)) {
            Logger::error << "getNumber() returned wrong value.  Expected: "
                          << class_value2 << " Received: " << class_value1
                          << "\n";
            return false;
        }
        Logger::debug << "  Class =  "
                      << obj.at("org.bluez.Device1").at("Class").getNumber()
                      << "\n";
    }
    return true;
}

static void print_properties_test(const DBusType& obj) {
    Logger::debug
        << "Testing functions by printing all interface properties.\n";

    for (auto& [interface_name, interface] : obj.getDict()) {
        Logger::debug << "    " << std::get<std::string>(interface_name)
                      << "\n";
        for (auto& [property_name, property_value] : interface.getDict()) {
            std::stringstream stream;
            stream << "        " << std::get<std::string>(property_name)
                   << " = ";
            stringify_property(property_value, stream);
            Logger::debug << stream.str();
        }
    }
}

// NOLINTEND(misc-use-anonymous-namespace)

// NOLINTNEXTLINE(bugprone-exception-escape)
int main() {

#ifdef NDEBUG
    Logger::setLogLevel(Logger::INFO);
#else
    Logger::setLogLevel(Logger::DEBUG);
#endif
    sd_bus* bus = nullptr; // NOLINT(misc-include-cleaner)
    if (const int res = sd_bus_default_system(&bus); res < 0) {
        print_err("sd_bus_default_system() returned error", -res);
        return 1;
    }

    sd_bus_error error{};            // NOLINT(misc-include-cleaner)
    sd_bus_message* reply = nullptr; // NOLINT(misc-include-cleaner)

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
    if (const int res = sd_bus_call_method(
            bus, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager",
            "GetManagedObjects", &error, &reply, nullptr);
        res < 0) {

        print_err("sd_bus_call_method() returned error", -res);
        Logger::error << "Is bluez installed?\n";
        return 1;
    }

    // If the following throws an exception, test will fail, which is what I
    // want. NOLINTNEXTLINE(bugprone-exception-escape)
    const DBusType processed_reply(*reply);

    if (const std::string& sig = processed_reply.getTypeSignature();
        !check_response_signature(sig)) {
        Logger::error << "Unexpected response signature received: " << sig
                      << "\n";
        Logger::error << "Expected" << expected_signature << "\n";
        return 1;
    }

    Logger::debug << "Response Type Signature: "
                  << processed_reply.getTypeSignature() << "\n";

    bool deviceFound = false;

    for (auto& [path, obj] : processed_reply.getDict()) {
        Logger::debug << "[ " << std::get<std::string>(path) << " ]\n";

        if (obj.getDict().contains("org.bluez.Device1")) {
            deviceFound = true;

            show_device_info(obj);
            try {
                if (!run_error_tests(obj)) {
                    return 1;
                }

                if (!get_number_test(obj)) {
                    return 1;
                }
            } catch (const std::exception& e) {
                Logger::error << "Unexpected exception thrown: " << e.what()
                              << "\n";
                return 1;
            }
        }

        try {
            print_properties_test(obj);
        } catch (const std::exception& e) {
            Logger::error << "Unexpected exception thrown: " << e.what()
                          << "\n";
            return 1;
        }
    }

    if (!deviceFound) {
        Logger::error << "No bluetooth devices were found.  Some functionality "
                         "untested.\n";
        return 1;
    }
}
