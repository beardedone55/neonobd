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

#include "event-handler.hpp"
#include "logger.hpp"
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>

// NOLINTBEGIN(misc-use-anonymous-namespace,misc-use-internal-linkage)
class TestEventHandler : public EventHandler {
  public:
    TestEventHandler() { init_event_handler(); }

    [[nodiscard]] int get_event_count() const { return m_event_count; }

    void reset_event_count() { m_event_count = 0; }

    void event_test1() {
        reset_event_count();
        signal_event(HWORLD_EVENT);
    }

    void event_test100() {
        reset_event_count();
        std::thread evt_thread([this]() { run_event_test100(); });
        evt_thread.detach();
    }

    void event_interleave_test() {
        reset_event_count();
        std::thread evt_thread(
            [this]() { run_event_interleave_test(HWORLD_EVENT); });
        std::thread evt_thread2(
            [this]() { run_event_interleave_test(GWORLD_EVENT); });
        evt_thread.detach();
        evt_thread2.detach();
    }

    void event_flood_test() {
        reset_event_count();
        m_threads_running = 2;
        m_stop_threads = false;
        std::thread evt_thread(
            [this]() { run_event_flood_test(HWORLD_EVENT); });
        std::thread evt_thread2(
            [this]() { run_event_flood_test(GWORLD_EVENT); });
        evt_thread.detach();
        evt_thread2.detach();
    }

    void stop_threads() { m_stop_threads = true; }
    [[nodiscard]] bool get_threads_stopped() const {
        return m_threads_running == 0;
    }

    static constexpr int SHORT_TEST_SIZE = 100;
    static constexpr int LONG_TEST_SIZE = 100000;

  protected:
    void process_event(std::string_view event) override {
        if (event != HWORLD_EVENT && event != GWORLD_EVENT) {
            Logger::error << "Unexpected event received: " << event << "\n";
            throw std::runtime_error("Unexpected event received: " +
                                     std::string(event));
        }
        ++m_event_count;
    }

  private:
    void run_event_test100() {
        for (int i = 0; i < SHORT_TEST_SIZE; ++i) {
            signal_event(HWORLD_EVENT);
        }
    }

    void run_event_interleave_test(const std::string& event) {
        for (int i = 0; i < LONG_TEST_SIZE; ++i) {
            signal_event(event);
        }
    }

    void run_event_flood_test(const std::string& event) {
        while (!m_stop_threads) {
            signal_event(event);
        }
        Logger::debug << "Flood event test exiting...\n";
        const std::scoped_lock lock(m_thread_lock);
        --m_threads_running;
    }

    static constexpr std::string HWORLD_EVENT = "Hello World!";
    static constexpr std::string GWORLD_EVENT = "Goodbye World!";
    int m_event_count = 0;
    volatile bool m_stop_threads = false;
    int m_threads_running = 0;
    std::mutex m_thread_lock;
};

static bool wait_for_events(TestEventHandler& event_handler, int count) {
    const int evt_handler_fd = event_handler.get_event_fd();
    epoll_event evt = {.events = EPOLLIN, .data = {.fd = evt_handler_fd}};
    const int epoll_fd = epoll_create1(0);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, evt_handler_fd, &evt);
    bool result = true;
    static constexpr int EPOLL_TIMEOUT = 100;
    while (event_handler.get_event_count() < count) {
        const int nfds = epoll_wait(epoll_fd, &evt, 1, EPOLL_TIMEOUT);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            Logger::error << "Error " << errno << " returned from epoll.\n";
            result = false;
            break;
        }

        if (nfds == 0) {
            Logger::error << "Timeout waiting for events.\n";
            result = false;
            break;
        }

        event_handler.process_events();
    }
    close(epoll_fd);
    return result;
}
// NOLINTEND(misc-use-anonymous-namespace,misc-use-internal-linkage)

int main(int argc, char* argv[]) {

#ifdef NDEBUG
    Logger::setLogLevel(Logger::INFO);
#else
    Logger::setLogLevel(Logger::DEBUG);
#endif

    int test_count = 1;

    const std::span args(argv, static_cast<size_t>(argc));

    if (args.size() > 1) {
        // We are doing the bounds checking with the if statement...
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        test_count = std::stoi(args[1]);
    }

    for (int i = 0; i < test_count; ++i) {
        TestEventHandler event_handler;

        Logger::debug << "Test pass " << i + 1 << "\n";

        Logger::debug << "Running Single Event Test.\n";

        event_handler.event_test1();
        event_handler.process_events();

        if (const int event_count = event_handler.get_event_count();
            event_count != 1) {
            Logger::error << "Unexpected event count.  Expected 1, received "
                          << event_count << "\n";
            return 1;
        }

        Logger::debug << event_handler.get_event_count()
                      << " events recorded.\n";
        Logger::debug << "Running " << TestEventHandler::SHORT_TEST_SIZE
                      << " Event Test.\n";

        event_handler.event_test100();

        if (!wait_for_events(event_handler,
                             TestEventHandler::SHORT_TEST_SIZE) ||
            event_handler.get_event_count() !=
                TestEventHandler::SHORT_TEST_SIZE) {
            Logger::error << TestEventHandler::SHORT_TEST_SIZE
                          << " Event test failed.\n";
            return 1;
        }

        Logger::debug << event_handler.get_event_count()
                      << " events recorded.\n";
        Logger::debug << "Running Event Interleave Test.\n";

        event_handler.event_interleave_test();

        static constexpr int LONG_TEST_SIZE_x2 =
            TestEventHandler::LONG_TEST_SIZE * 2;

        if (!wait_for_events(event_handler, LONG_TEST_SIZE_x2) ||
            event_handler.get_event_count() != LONG_TEST_SIZE_x2) {
            Logger::error << "Event interleave test failed. Event count = "
                          << event_handler.get_event_count() << "\n";
            return 1;
        }

        Logger::debug << event_handler.get_event_count()
                      << " events recorded.\n";
        Logger::debug << "Running Event Flood Test.\n";

        event_handler.event_flood_test();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (!wait_for_events(event_handler, LONG_TEST_SIZE_x2)) {
            Logger::error << "Event flood test failed.\n";
            return 1;
        }

        Logger::debug << event_handler.get_event_count()
                      << " events recorded.\n";

        event_handler.stop_threads();

        while (!event_handler.get_threads_stopped()) {
            event_handler.reset_event_count();
            wait_for_events(event_handler, LONG_TEST_SIZE_x2);
        }

        Logger::debug << event_handler.get_event_count()
                      << " events recorded.\n";
    }

    return 0;
}
