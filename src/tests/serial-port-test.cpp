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
 * It is an interactive test, and it requires access to a serial
 * device in order to function.
 *
 * An automated version of this test would likely require some sort of driver
 * to emulate the bluetooth device.  I'll work on that later.
 */

#include "hardware-interface.hpp"
#include "logger.hpp"
#include "serial-port.hpp"
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <thread>

// NOLINTBEGIN(misc-use-anonymous-namespace)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool device_connected = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool device_connecting = true;

static void connect_complete(bool complete) {
    device_connecting = false;
    device_connected = complete;
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

static void reader_thread(HardwareInterface& hwif) {
    std::string buf;
    while (!stop_reader) {
        while (hwif.read(buf) > 0) {
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
    SerialPort serial_port;

    Logger::debug << "Serial port devices:\n";
    auto devices = SerialPort::get_serial_devices();
    for (const auto& device : devices) {
        Logger::debug << "   " << device << "\n";
    }
    if (devices.empty()) {
        Logger::error << "No Serial Port Devices found...\n";
        return 1;
    }

    const std::string device =
        select_from_list(devices, {.title = "Serial Port Devices Found...",
                                   .prompt = "Select device"});

    auto baudrates = SerialPort::get_valid_baudrates();

    if (baudrates.empty()) {
        Logger::error << "No valid baudrates found...\n";
        return 1;
    }

    const std::string baudrate =
        select_from_list(baudrates, {.title = "Serial Port Baudrates",
                                     .prompt = "Select Baudrate"});

    serial_port.set_baudrate(baudrate);

    Logger::debug << "Selected Serial Port " << device << "\n";

    if (!serial_port.connect(device, connect_complete)) {
        Logger::error << "Connection failed!\n";
        return 1;
    }

    while (device_connecting) {
        try {
            wait_for_response(serial_port);
        } catch (const std::exception& e) {
            return 1;
        }
        serial_port.process_events();
    }

    if (!device_connected) {
        Logger::error << "Connection timed out.\n";
        return 1;
    }

    std::thread thread(reader_thread, std::ref(serial_port));

    std::string user_input;

    std::cout << "Connection to serial device created.\n";
    std::cout << "Type \"exit\" to quit.\n";

    while (true) {
        std::getline(std::cin, user_input);
        if (user_input == "exit") {
            break;
        }

        serial_port.write(user_input + "\r");
    }

    stop_reader = true;
    thread.join();

    return 0;
}
