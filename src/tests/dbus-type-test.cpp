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
#include <cstring>
#include <sstream>
#include <string>
#include <systemd/sd-bus.h>

static void print_err(const std::string& msg, int err) {
    Logger::error << msg << ": " << std::strerror(err) << "(" << err << ")\n";
}

static const std::string expected_signature = "a{oa{sa{sv}}}";

static bool check_response_signature(const std::string& sig) {
    return sig == expected_signature;
}

static void process_array(const DBusType& t, std::stringstream& ss) {
    std::vector<DBusType> a;
    try {
        a = t.getArray();
    } catch(std::bad_variant_access& e) {
        ss << "[]\n";
        return;
    }
    
    ss << "[\n                         ";

    for(auto& v : a) {
        if(v.getType() == "s") {
            ss << v.getString();
        } else {
            ss << v.getNumber();
        }
        ss << "\n                         ";
    }

    ss << "\b\b]\n";
}

int main() {
    sd_bus* bus = nullptr;
    if(int r=sd_bus_default_system(&bus); r < 0) {
        print_err("sd_bus_default_system() returned error", -r);
        return 1;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    if(int r=sd_bus_call_method(bus, "org.bluez", "/",
                "org.freedesktop.DBus.ObjectManager",
                "GetManagedObjects", &error, &reply, nullptr); r < 0) {

        print_err("sd_bus_call_method() returned error", -r);
        Logger::error << "Is bluez installed?\n";
        return 1;
    }

    DBusType processed_reply(*reply);

    if(std::string sig = processed_reply.getTypeSignature(); !check_response_signature(sig)) {
        Logger::error << "Unexpected response signature received: " << sig << "\n";
        Logger::error << "Expected" << expected_signature << "\n";
        return 1;
    }

    Logger::debug << "Response Type Signature: " << processed_reply.getTypeSignature() << "\n";

    bool deviceFound = false;

    for(auto& [path, obj] : processed_reply.getDict()) {
        Logger::debug << "[ " << std::get<std::string>(path) << " ]\n";

        if(obj.getDict().contains("org.bluez.Device1")) {
            deviceFound = true;
            Logger::debug << "This is a bluetooth device!\n";
            Logger::debug << "Its device address is " << obj["org.bluez.Device1"]["Address"].getValue<std::string>() << "\n";
            Logger::debug << "Its first UUID is " << obj["org.bluez.Device1"]["UUIDs"][0].getString() << "\n";
            auto paired_bool = obj["org.bluez.Device1"]["Paired"].getBool();
            auto paired_number = obj["org.bluez.Device1"]["Paired"].getNumber();
            
            Logger::debug << "Testing getNumber() on boolean type.\n";
            if(paired_bool != !!paired_number) {
                Logger::error << "getNumber() returned wrong value.  Expected: " << paired_bool << " Received: "
                    << paired_number << "\n";
                return 1;
            }

            Logger::debug << "  Paired =  " << std::boolalpha << paired_bool << "\n";
            Logger::debug << "  Paired =  " << std::boolalpha << paired_number << "\n";

            Logger::debug << "Testing getBasicType() on boolean type.\n";
            auto paired_basic_type = obj["org.bluez.Device1"]["Paired"].getBasicType();

            if(paired_bool != std::get<bool>(paired_basic_type)) {
                Logger::error << "getBasicType() returned wrong value.  Expected: " << paired_bool << " Received: "
                    << std::get<bool>(paired_basic_type) << "\n";
                return 1;
            }

            Logger::debug << "Testing getBasicType() on container type.\n";
            try {
                obj["org.bluez.Device1"].getBasicType();
                Logger::error << " getBasicType() should have thrown exception for container type.\n";
                return 1;
            } catch(const std::bad_variant_access& e) {
                Logger::debug << " Device interface is not a basic type.\n";
            }

            Logger::debug << "Testing getString() on boolean type.\n";
            try {
                obj["org.bluez.Device1"]["Paired"].getString();
                Logger::error << " getString() should have thrown exception for Paired property\n";
                return 1;
            } catch(const std::bad_variant_access& e) {
                Logger::debug << " Paired property is not a string.\n";
            }

            Logger::debug << "Testing getNumber() on string type.\n";
            try {
                obj["org.bluez.Device1"]["Address"].getNumber();
                Logger::error << " getNumber() should have thrown exception for Address property\n";
                return 1;
            } catch(const std::bad_variant_access& e) {
                Logger::debug << " Address property is not a number.\n";
            }

            Logger::debug << "Testing getValue<std::uint32_t>() on boolean type.\n";
            try {
                obj["org.bluez.Device1"]["Paired"].getValue<std::uint32_t>();
                Logger::error << " getValue<std::uint32_t>() should have thrown exception for Paired property\n";
                return 1;
            } catch(const std::bad_variant_access& e) {
                Logger::debug << " Paired property is a boolean, not a uint32_t.\n";
            }


            if(obj["org.bluez.Device1"].getDict().contains("Class")) {
                Logger::debug << "Testing getNumber() and getValue<std::uint32_t>() on uint32_t type.\n";
                auto class_value1 = obj["org.bluez.Device1"]["Class"].getNumber();
                auto class_value2 = obj["org.bluez.Device1"]["Class"].getValue<std::uint32_t>();
                if(class_value1 != class_value2) {
                    Logger::error << "getNumber() returned wrong value.  Expected: " << class_value2 << " Received: "
                        << class_value1 << "\n";
                    return 1;
                }
                Logger::debug << "  Class =  " << obj["org.bluez.Device1"]["Class"].getNumber() << "\n";
            }
            
            
        }
        Logger::debug << "Testing functions by printing all interface properties.\n";

        for(auto& [interface_name, interface] : obj.getDict()) {
            Logger::debug << "    " << std::get<std::string>(interface_name) << "\n";
            for(auto& [property_name, property_value] : interface.getDict()) {
                std::stringstream ss;
                ss << "        " << std::get<std::string>(property_name) << " = ";
                switch(property_value.getType()[0]) {
                    case 'b':
                        ss << std::boolalpha << property_value.getValue<bool>() << "\n";
                        break;
                    case 'y':
                        ss << static_cast<int>(property_value.getValue<uint8_t>()) << "\n";
                        break;
                    case 'n':
                        ss << property_value.getValue<int16_t>() << "\n";
                        break;
                    case 'q':
                        ss << property_value.getValue<uint16_t>() << "\n";
                        break;
                    case 'i':
                    case 'h':
                        ss << property_value.getValue<int32_t>() << "\n";
                        break;
                    case 'u':
                        ss << property_value.getValue<uint32_t>() << "\n";
                        break;
                    case 'x':
                        ss << property_value.getValue<int64_t>() << "\n";
                        break;
                    case 't':
                        ss << property_value.getValue<uint64_t>() << "\n";
                        break;
                    case 'd':
                        ss << property_value.getValue<double>() << "\n";
                        break;
                    case 's':
                    case 'o':
                        ss << property_value.getValue<std::string>() << "\n";
                        break;
                    case 'a':
                        process_array(property_value, ss);
                        break;
                    default:
                        ss << "[]\n";
                }
                Logger::debug << ss.str();
            }
        }
    }

    if(!deviceFound) {
        Logger::error << "No bluetooth devices were found.  Some functionality untested.\n";
        return 1;
    }
}
