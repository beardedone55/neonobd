/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2026  Brian LePage
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

/* This program tests the functionality of the BluetoothSerial port class.
 * It is an interactive test, and it requires access to a bluetooth serial
 * device in order to function.
 *
 * An automated version of this test would likely require some sort of driver
 * to emulate the bluetooth device.  I'll work on that later.
 */

#include "serial-port.hpp"
#include "logger.hpp"
#include <poll.h>
#include <thread>

static bool device_connected = false;
static bool device_connecting = true;

void connect_complete(bool complete) {
    device_connecting = false;
    device_connected = complete;
}

static auto select_from_list(const auto& list, std::string_view title, std::string_view prompt) {
    do {
        std::cout << "\n" << title << "\n";
        for(int i=0; i < list.size(); ++i) {
            std::cout << "   " << i+1 << ") " << list[i] << "\n";
        }

        std::cout << "\n" << prompt << " (1-" << list.size() << "): ";
        int selection;
        std::cin >> selection;
        if(std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if(selection > 0 && selection <= list.size()) {
            return list[selection-1];
        }
    } while(true);
    
}

static bool stop_reader = false;

void reader_thread(HardwareInterface& sp) {
    std::string buf;
    while(!stop_reader) {
    while(sp.read(buf) > 0) {
        std::cout << buf << std::flush;
    }
    }
}

void wait_for_response(HardwareInterface& hwif) {
    Logger::debug << "Entered wait_for_response.\n";
    pollfd pfd = { .fd = hwif.get_event_fd(), .events = POLLIN };
    int ret = poll(&pfd, 1, -1); 
    if(ret <=0) {
        Logger::error << "Poll returned " << ret << "\n";
        throw std::runtime_error("Poll returned an error!");
    }
}   

int main() {
    SerialPort sp;

    sp.process_events(1000us);

    Logger::debug << "Serial port devices:\n";
    auto devices = sp.get_serial_devices();
    for(const auto& device : devices) {
        Logger::debug << "   " << device << "\n";
    }
    if(devices.empty()) {
        Logger::error << "No Serial Port Devices found...\n";
        return 1;
    }
    
    std::string device = select_from_list(devices, "Serial Port Devices Found...", "Select device");

    auto baudrates = sp.get_valid_baudrates();

    if(baudrates.empty()) {
        Logger::error << "No valid baudrates found...\n";
        return 1;
    }

    std::string baudrate = select_from_list(baudrates, "Serial Port Baudrates", "Select Baudrate");

    sp.set_baudrate(baudrate);

    Logger::debug << "Selected Serial Port " << device << "\n";

    if(!sp.connect(device, connect_complete)) {
        Logger::error << "Connection failed!\n";
        return 1;
    }

    while(device_connecting) {
        wait_for_response(sp);
        sp.process_events(0s);
    }

    if(!device_connected) {
        Logger::error << "Connection timed out.\n";
        return 1;
    }

    std::thread t(reader_thread, std::ref(sp));

    std::string user_input;
    
    std::cout << "Connection to serial device created.\n";
    std::cout << "Type \"exit\" to quit.\n";

    while(true) {
        std::getline(std::cin, user_input);
        if(user_input == "exit") {
            break;
        }

        sp.write(user_input + "\r");
    }

    stop_reader = true;
    t.join();

    return 0;

}
