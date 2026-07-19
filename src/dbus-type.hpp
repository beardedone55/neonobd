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

#pragma once
#include "logger.hpp"
#include <concepts>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <systemd/sd-bus.h>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

// DBusBasicType represents a DBus type that is not a container.

using DBusBasicType =
    std::variant<std::monostate, std::uint8_t, bool, std::int16_t,
                 std::uint16_t, std::int32_t, std::uint32_t, std::int64_t,
                 std::uint64_t, double, std::string>;

// DBusType represents all possible DBus types, including containers that may
// contain DBusType objects.  The DBusType is constructed from an
// sd_bus_message. (See systemd/sd-bus.h .)

class DBusType {
  public:
    // construct DBusType from sd_bus_message.
    explicit DBusType(sd_bus_message& msg)
        : m_type_signature(sd_bus_message_get_signature(&msg, 0)) {
        const char* contents;
        char type;
        sd_bus_message_peek_type(&msg, &type, &contents);
        m_type = std::string(1, type);
        switch (type) {
        case 'y':
            read_value<std::uint8_t>(msg, m_type);
            break;
        case 'b':
            read_bool_value(msg);
            break;
        case 'n':
            read_value<std::int16_t>(msg, m_type);
            break;
        case 'q':
            read_value<std::uint16_t>(msg, m_type);
            break;
        case 'i':
        case 'h':
            read_value<std::int32_t>(msg, m_type);
            break;
        case 'u':
            read_value<std::uint32_t>(msg, m_type);
            break;
        case 'x':
            read_value<std::int64_t>(msg, m_type);
            break;
        case 't':
            read_value<std::uint64_t>(msg, m_type);
            break;
        case 'd':
            read_value<double>(msg, m_type);
            break;
        case 's':
        case 'o':
            read_string(msg, m_type);
            break;
        case 'v':
            read_variant(msg, contents);
            m_type = contents;
            break;
        case 'a':
        case 'r':
            read_container(msg, m_type, contents);
            break;
        default:
            throw std::invalid_argument(
                "DBus message contains an invalid type.\n");
        }
    }

    // The following getters will extract a C++ type from the DBus type.
    // It is expected that the caller knows what C++ type is represented by
    // the data.  An exception will be thrown if the caller requests an integer
    // when the object contains a string, for example.

    // Extract specified C++ type from DBus type.
    template <typename T> T getValue() const { return std::get<T>(m_value); }

    // Extract C++ vector type from DBus type.
    std::vector<DBusType> getArray() const {
        return std::get<std::vector<DBusType>>(m_value);
    }

    // Extract C++ map type from DBus type.
    std::unordered_map<DBusBasicType, DBusType> getDict() const {
        return std::get<std::unordered_map<DBusBasicType, DBusType>>(m_value);
    }

    // Extract C++ string type from DBus type.
    std::string getString() const { return getValue<std::string>(); }

    // Extract C++ integer type from DBus type.
    long getNumber() const {
        return std::visit(
            [](const auto& a) {
                long result = 0;
                if constexpr (std::is_integral_v<std::decay_t<decltype(a)>>) {
                    result = static_cast<long>(a);
                    return result;
                }
                throw std::bad_variant_access();
                return result;
            },
            m_value);
    }

    // Extract C++ boolean type from DBus type.
    bool getBool() const { return getValue<bool>(); }

    // Convert DBusType to DBusBasicType if possible.
    DBusBasicType getBasicType() const {
        if (m_basic_value.index() == 0) {
            throw std::bad_variant_access();
        }
        return m_basic_value;
    }

    const std::string& getTypeSignature() const { return m_type_signature; }

    const std::string& getType() const { return m_type; }

    DBusType at(const std::integral auto& i) const {
        if (std::holds_alternative<std::vector<DBusType>>(m_value)) {
            return getArray().at(i);
        } else {
            return getDict().at(i);
        }
    }

    DBusType at(const std::string& i) const { return getDict().at(i); }

  private:
    using DBusVariant =
        std::variant<std::uint8_t, bool, std::int16_t, std::uint16_t,
                     std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                     double, std::string, std::vector<DBusType>,
                     std::unordered_map<DBusBasicType, DBusType>>;

    DBusVariant m_value;
    DBusBasicType m_basic_value;
    std::string m_type_signature;
    std::string m_type;

    template <typename T>
    static T extract_value(sd_bus_message& msg, std::string_view type) {
        T v;
        sd_bus_message_read(&msg, type.data(), &v);
        return v;
    }

    template <typename T>
    void read_value(sd_bus_message& msg, std::string_view type) {
        T v = extract_value<T>(msg, type);
        m_value = v;
        m_basic_value = v;
    }

    void read_bool_value(sd_bus_message& msg) {
        bool v = extract_value<int>(msg, "b") != 0;
        m_value = v;
        m_basic_value = v;
    }

    void read_string(sd_bus_message& msg, std::string_view type) {
        std::string v = extract_value<const char*>(msg, type);
        m_value = v;
        m_basic_value = v;
    }

    void read_variant(sd_bus_message& msg, std::string_view type) {
        sd_bus_message_enter_container(&msg, 'v', type.data());
        DBusType v(msg);
        sd_bus_message_exit_container(&msg);
        m_value = std::move(v.m_value);
        m_basic_value = std::move(v.m_basic_value);
    }

    static std::vector<DBusType> read_array(sd_bus_message& msg) {
        std::vector<DBusType> result;
        while (sd_bus_message_peek_type(&msg, nullptr, nullptr) > 0) {
            result.emplace_back(msg);
        }
        return result;
    }

    static std::unordered_map<DBusBasicType, DBusType>
    read_dict(sd_bus_message& msg, const char type, std::string_view contents) {
        std::unordered_map<DBusBasicType, DBusType> result;
        while (sd_bus_message_enter_container(&msg, type, contents.data()) >
               0) {
            DBusType key(msg);
            DBusType value(msg);
            result.emplace(key.getBasicType(), value);
            sd_bus_message_exit_container(&msg);
        }
        return result;
    }

    void read_container(sd_bus_message& msg, std::string_view type,
                        std::string_view contents) {
        sd_bus_message_enter_container(&msg, type[0], contents.data());
        char element_type;
        const char* element_contents;
        if (sd_bus_message_peek_type(&msg, &element_type, &element_contents) >
            0) {
            if (type[0] == 'a' && element_type == SD_BUS_TYPE_DICT_ENTRY) {
                m_value = read_dict(msg, element_type, element_contents);
            } else {
                m_value = read_array(msg);
            }
        } else if (contents[0] == '{') {
            m_value = std::unordered_map<DBusBasicType, DBusType>();
        } else {
            m_value = std::vector<DBusType>();
        }
        sd_bus_message_exit_container(&msg);
    }
};
