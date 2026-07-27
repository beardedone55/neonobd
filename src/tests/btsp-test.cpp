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
#include "hardware-interface.hpp"
#include "logger.hpp"
#include "neonobd_types.hpp"
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// NOLINTBEGIN(misc-use-anonymous-namespace)

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool scan_complete = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool device_connected = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool device_connecting = true;

static void progress_bar(int percent_complete) {
    std::cout << percent_complete << "%\r" << std::flush;
    static constexpr int TASK_COMPLETE = 100;
    if (percent_complete == TASK_COMPLETE) {
        scan_complete = true;
    }
}

static void connect_complete(bool complete) {
    device_connecting = false;
    device_connected = complete;
}

static void user_input(BluetoothSerialPort& btsp, const std::string& message,
                       const ResponseType type, void* handle) {

    ResponseVariant response;

    std::cout << message << "\n";

    if (type == neon::USER_YN) {
        bool resp = false;
        std::cin >> resp;
        response = resp;
    } else if (type == neon::USER_INT) {
        int resp = 0;
        std::cin >> resp;
        response = resp;
    } else if (type == neon::USER_STRING) {
        std::string resp;
        std::cin >> resp;
        response = resp;
    }

    btsp.respond_from_user(response, handle);
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
struct SelectionPrompt {
    std::string_view title;
    std::string_view prompt;
};

static auto select_from_list(const auto& list, const SelectionPrompt& prompt) {
    do {
        std::cout << "\n" << prompt.title << "\n";
        for (size_t i = 0; i < list.size(); ++i) {
            std::cout << "   " << i + 1 << ") " << list.at(i) << "\n";
        }

        std::cout << "\n" << prompt.prompt << " (1-" << list.size() << "): ";
        size_t selection = 0;
        std::cin >> selection;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (selection <= list.size()) {
            return list.at(selection - 1);
        }
    } while (true);
}
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool stop_reader = false;

static void reader_thread(HardwareInterface& btsp) {
    std::string buf;
    while (!stop_reader) {
        while (btsp.read(buf) > 0) {
            std::cout << buf << std::flush;
        }
    }
}

static void wait_for_response(const HardwareInterface& hwif) {
    Logger::debug << "Entered wait_for_response.\n";
    pollfd pfd = {.fd = hwif.get_event_fd(), .events = POLLIN, .revents = 0};
    const int ret = poll(&pfd, 1, -1);
    if (ret <= 0) {
        Logger::error << "Poll returned " << ret << "\n";
        throw std::runtime_error("Poll returned an error!");
    }
}

// NOLINTEND(misc-use-anonymous-namespace)

int main() {
    BluetoothSerialPort btsp;

    // NOLINTNEXTLINE(misc-include-cleaner)
    std::this_thread::sleep_for(1s);

    btsp.process_events();

    Logger::debug << "Bluetooth Controllers:\n";
    auto controllers = btsp.get_controller_names();
    for (const auto& ctlr : controllers) {
        Logger::debug << "   " << ctlr << "\n";
    }
    if (controllers.empty()) {
        Logger::error << "No Bluetooth Controllers found...\n";
        return 1;
    }

    const std::string ctlr = select_from_list(
        controllers, {.title = "Bluetooth Controllers Found...",
                      .prompt = "Select controller"});

    btsp.select_controller(ctlr);

    Logger::debug << "Selected Bluetooth Controller " << ctlr << "\n";

    try {
        btsp.connect_user_input(
            [&](const std::string& msg, ResponseType type, void* handle) {
                user_input(btsp, msg, type, handle);
            });
    } catch (const std::exception& e) {
        Logger::error << "Unexpected exception connecting user input: "
                      << e.what() << "\n";
        return 1;
    }

    btsp.probe_remote_devices(progress_bar);

    while (!scan_complete) {
        try {
            wait_for_response(btsp);
        } catch (const std::exception& e) {
            return 1;
        }
        btsp.process_events();
    }

    auto devices = btsp.get_device_names_addresses();
    if (devices.empty()) {
        Logger::error << "No Bluetooth Devices found....\n";
        return 1;
    }

    std::vector<std::string> device_names;
    device_names.reserve(devices.size());
    // cppcheck-suppress-begin useStlAlgorithm
    for (const auto& dev : devices) {
        device_names.push_back(dev.address + " : " + dev.name);
    }
    // cppcheck-suppress-end useStlAlgorithm

    auto selected_device =
        select_from_list(device_names, {.title = "Bluetooth Devices Found:",
                                        .prompt = "Select device"});

    std::stringstream device_stream(selected_device);
    std::string device_address;
    device_stream >> device_address;

    if (!btsp.connect(device_address, connect_complete)) {
        Logger::error << "Connection failed!\n";
        return 1;
    }

    while (device_connecting) {
        btsp.process_events();
    }

    if (!device_connected) {
        Logger::error << "Connection timed out.\n";
        return 1;
    }

    std::thread thread(reader_thread, std::ref(btsp));

    std::string input;

    std::cout << "Connection to serial device created.\n";
    std::cout << "Type \"exit\" to quit.\n";

    while (true) {
        std::getline(std::cin, input);
        if (input == "exit") {
            break;
        }

        btsp.write(input + "\r");
    }

    stop_reader = true;
    btsp.disconnect(nullptr);
    btsp.process_events();
    thread.join();

    return 0;
}
