/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2024  Brian LePage
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

#include "obd.hpp"
#include "hardware-interface.hpp"
#include "neonobd_exceptions.hpp"
#include "obd-device.hpp"
#include <memory>
#include <sigc++/signal.h>


//Service $01: Power train and diagnostic data.
//Service $02: Power train freeze frame data.

typedef std::vector<Obd::ResultTypes>(Obd::*CompletionCallback)(std::span<unsigned char>&); 

constexpr std::array<CompletionCallback,100> service_1_pid_table {
    nullptr,
    &Obd::get_MIL_status_complete
}

constexpr std::array<std::array<Obd::CommandCompleteFunction>*,10> pid_request_table {
    nullptr,
    &service_1_pid_table,  //Service $01
    &service_1_pid_table,  //Service $02

};

sigc::signal<void(bool)>
Obd::init(const std::shared_ptr<ObdDevice>& obd_device,
          const std::shared_ptr<HardwareInterface>& hwif) {

    if (m_connected || m_connecting) {
        throw neon::InvalidState("Invalid state to initialize OBD device.");
    }

    m_obdDevice = obd_device;
    m_hwif = hwif;

    m_init_connection = obd_device->init(hwif).connect(
        [this](bool success) { initComplete(success); });

    m_connecting = true;

    return m_init_signal;
}

void Obd::initComplete(bool success) {
    m_connected = success;
    m_connecting = false;
    m_is_CAN = m_obdDevice->is_CAN();
    m_init_connection.disconnect();
    if(success) {
        m_command_connection = 
            m_obdDevice->signal_command_complete().connect(
                [this](const auto& result){command_complete(result);});
    }
    
    m_init_signal.emit(success);
}

sigc::signal<void()> Obd::disconnect() {
    if (!m_connected || disconnecting) {
        throw neon::InvalidState("Invalid state to disconnect OBD device.");
    }
    m_disconnect_connection =
        m_obdDevice->disconnect().connect([this]() { disconnectComplete(); });
    return m_disconnect_signal;
}

void Obd::disconnectComplete() {
    m_disconnect_connection.disconnect();
    m_disconnect_signal.emit();
}

void Obd::command_complete(const std::unordered_map<unsigned int, std::vector<unsigned char>>& result) {
    auto& callback = m_command_queue.front().second;
    auto& [ecu, data] = *result.begin();
    auto processed_result = callback(std::span(data));
    m_command_queue.front().first.emit(processed_result);
    m_command_queue.pop();
}

Obd::ResultSignal Obd::get_PID(unsigned char ecu, unsigned char service, unsigned char pid) {
    ResultSignal command_complete_signal;
    auto& callback = pid_request_table[service][pid];
    m_command_queue.push(std::make_pair(command_complete_signal, [this](auto data){std::invoke(this,callback, data)});
    m_obdDevice->send_command(ecu, service, {pid});
    return command_complete_signal;
}

namespace {
void process_monitor_statuses(
    std::vector<Obd::ResultTypes>& fmt_result,
    unsigned char supported,
    unsigned char ready,
    int ready_start,
    int size) {

    for(int i=0; i < size; ++i) {
        if((supported >> i) & 1 == 0) {
            fmt_result.push_back("NOT SUPPORTED");
            continue;
        }
        if((ready >> (i+ready_start)) & 1 == 1 {
            fmt_result.push_back("READY");
        } else {
            fmt_result.push_back("NOT READY");
        }
    }
}

bool check_response(std::span<unsigned char>& data, 
                    int expected_size,
                    unsigned char expected_response) {
    if(expected_size > data.size() ||
       data[0] != expected_response) {
        data = data.subspan(data.size());
        return false;
    }
    return true;
}

}

std::vector<Obd::ResultTypes> Obd::get_MIL_status_complete(std::span<unsigned char>& data) {
    constexpr unsigned char GET_MIL_POSITIVE_RESPONSE = 0x41;
    constexpr int DATA_SIZE = 5;
    if(!check_response(data, DATA_SIZE, GET_MIL_POSITIVE_RESPONSE)) {
        return {"ERROR"};
    }
    constexpr unsigned char MIL_MASK = 0x80;
    std::vector<ResultTypes> fmt_result;
    //MIL Status and DTC Count (Byte A)
    fmt_result.push_back((data[1] & MIL_MASK) == MIL_MASK);
    fmt_result.push_back(data[1] & ~MIL_MASK);
    //Continuous Monitor Statuses (Misfire, Fuel System, Comprehensive)
    process_monitor_statuses(fmt_result, data[2], data[2], 4, 3);
    //Once-per-trip Monitor Statuses (Bytes C/D)
    process_monitor_statuses(fmt_result, data[3], data[4], 0, 8);
    data = data.subspan(DATA_SIZE);
    return processedResult;
}


