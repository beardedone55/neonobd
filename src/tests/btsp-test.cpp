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

#include "bluetooth-serial-port.hpp"
#include "logger.hpp"
#include <poll.h>
#include <thread>

static bool scan_complete = false;
static bool device_connected = false;
static bool device_connecting = true;

void progress_bar(int percent_complete) {
    std::cout << percent_complete << "%\r" << std::flush;
    if(percent_complete == 100) {
        scan_complete = true;
    }
}

void connect_complete(bool complete) {
    device_connecting = false;
    device_connected = complete;
}

void user_input(BluetoothSerialPort& btsp, const std::string& message, const ResponseType type, void* handle) {
 
    ResponseVariant response;   

    std::cout << message << "\n";

    if(type == neon::USER_YN) {
        bool r;
        std::cin >> r;
        response = r;
    } else if (type == neon::USER_INT) {
        int r;
        std::cin >> r;
        response = r;
    } else if (type == neon::USER_STRING) {
        std::string r;
        std::cin >> r;
        response = r;
    }

    btsp.respond_from_user(response, handle);
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

void reader_thread(HardwareInterface& btsp) {
    std::string buf;
    while(!stop_reader) {
    while(btsp.read(buf) > 0) {
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
    BluetoothSerialPort btsp;

    btsp.process_events(1000us);

    Logger::debug << "Bluetooth Controllers:\n";
    auto controllers = btsp.get_controller_names();
    for(const auto& ctlr : controllers) {
        Logger::debug << "   " << ctlr << "\n";
    }
    if(controllers.empty()) {
        Logger::error << "No Bluetooth Controllers found...\n";
        return 1;
    }
    
    std::string ctlr = select_from_list(controllers, "Bluetooth Controllers Found...", "Select controller");

    btsp.select_controller(ctlr);

    Logger::debug << "Selected Bluetooth Controller " << ctlr << "\n";

    btsp.connect_user_input(
            [&](const std::string& msg, ResponseType type, void* handle) {
                user_input(btsp, msg, type, handle);
            });

    btsp.probe_remote_devices(progress_bar);

    while(!scan_complete) {
        wait_for_response(btsp);
        btsp.process_events(0s);
    }

    auto devices  = btsp.get_device_names_addresses();
    if(devices.empty()) {
        Logger::error << "No Bluetooth Devices found....\n";
        return 1;
    }

    std::vector<std::string> device_names;
    for(const auto& d : devices) {
        device_names.push_back(d.address + " : " + d.name);
    }

    auto selected_device = select_from_list(device_names, "Bluetooth Devices Found:", "Select device");

    std::stringstream device_stream(selected_device);
    std::string device_address;
    device_stream >> device_address;

    if(!btsp.connect(device_address, connect_complete)) {
        Logger::error << "Connection failed!\n";
        return 1;
    }

    while(device_connecting) {
        btsp.process_events(1000us);
    }

    if(!device_connected) {
        Logger::error << "Connection timed out.\n";
        return 1;
    }

    std::thread t(reader_thread, std::ref(btsp));

    std::string user_input;
    
    std::cout << "Connection to serial device created.\n";
    std::cout << "Type \"exit\" to quit.\n";

    while(true) {
        std::getline(std::cin, user_input);
        if(user_input == "exit") {
            break;
        }

        btsp.write(user_input + "\r");
    }

    stop_reader = true;
    btsp.disconnect(nullptr);
    btsp.process_events(1000us);
    t.join();

    return 0;

}
