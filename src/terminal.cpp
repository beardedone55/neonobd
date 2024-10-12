/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2024  Brian LePage
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

#include "terminal.hpp"
#include "logger.hpp"
#include "mainwindow.hpp"
#include <chrono>
#include <gtkmm/button.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <memory>
#include <mutex>
#include <sigc++/functors/mem_fun.h>

Terminal::Terminal(MainWindow* main_window)
    : window{main_window},
      visibleView{window->viewStack->property_visible_child_name()} {
    auto user_interface = window->ui;
    visibleView.signal_changed().connect(
        sigc::mem_fun(*this, &Terminal::on_show));

    homeButton =
        user_interface->get_widget<Gtk::Button>("terminal_home_button");
    homeButton->signal_clicked().connect(
        sigc::mem_fun(*this, &Terminal::homeClicked));

    terminal = user_interface->get_widget<Gtk::TextView>("terminal_text_view");
    textBuffer = Gtk::TextBuffer::create();
    terminal->set_buffer(textBuffer);
    textBuffer->signal_insert().connect(
        sigc::mem_fun(*this, &Terminal::textEntered));
    tagReadOnly = textBuffer->create_tag();
    tagReadOnly->property_editable() = false;
    inputBegin = textBuffer->create_mark(textBuffer->begin());
    textBuffer->property_cursor_position().signal_changed().connect(
        sigc::mem_fun(*this, &Terminal::cursorMoved));

    dispatcher.connect(sigc::mem_fun(*this, &Terminal::reader_notification));
}

Terminal::~Terminal() {
    stop_reader = true;
    if (reader_thread && reader_thread->joinable()) {
        reader_thread->join();
    }
}

using namespace std::chrono_literals;

void Terminal::read_data() {
    Logger::debug("Terminal reader thread started.");
    auto hwif = window->hardwareInterface;
    // std::operation""ms is included in <chrono> according to c++ docs.
    // NOLINTNEXTLINE(misc-include-cleaner)
    static constexpr std::chrono::milliseconds timeout = 100ms;
    hwif->set_timeout(timeout);
    std::string tempBuffer;
    while (!stop_reader) {
        auto bytecount = hwif->read(tempBuffer);
        if (bytecount > 0) {
            const std::lock_guard lock(read_buffer_mutex);
            read_buffer.append(tempBuffer);
            dispatcher.emit();
        }
    }
    reader_stopped = true;
    Logger::debug("Terminal reader thread stopped.");
}

void Terminal::start_reader_thread() {
    if (reader_thread && reader_thread->joinable()) {
        reader_thread->join();
    }

    reader_stopped = false;
    read_buffer.resize(0);
    reader_thread =
        std::make_unique<std::thread>([this]() { this->read_data(); });
}

void Terminal::reader_notification() {
    if (reader_stopped) {
        reader_thread->join();
        if (!stop_reader) {
            start_reader_thread();
        }
        return;
    }

    const std::lock_guard lock(read_buffer_mutex);
    auto pos = textBuffer->insert(inputBegin->get_iter(), read_buffer);
    lock_text(pos);
    terminal->scroll_to(inputBegin);
    read_buffer.resize(0);
}

void Terminal::lock_text(const Gtk::TextBuffer::iterator& pos) {
    textBuffer->apply_tag(tagReadOnly, textBuffer->begin(), pos);
    textBuffer->move_mark(inputBegin, pos);
}

void Terminal::on_show() {
    if (visibleView.get_value() != "terminal_view") {
        return;
    }

    terminal->grab_focus();
    textBuffer->set_text(">");
    lock_text(textBuffer->end());

    stop_reader = false;

    if (!reader_stopped) {
        return;
    }

    start_reader_thread();
}

void Terminal::homeClicked() {
    stop_reader = true;
    window->viewStack->set_visible_child("home_view");
}

void Terminal::cursorMoved() {
    if (textBuffer->get_insert()->get_iter() < inputBegin->get_iter()) {
        textBuffer->place_cursor(inputBegin->get_iter());
    }
}

void Terminal::textEntered(Gtk::TextBuffer::iterator& pos,
                           const Glib::ustring& text, int /*bytes*/) {
    if (text == "\n" && pos > inputBegin->get_iter()) {
        textBuffer->backspace(pos);
        std::string user_input =
            textBuffer->get_text(inputBegin->get_iter(), textBuffer->end());
        Logger::debug("User input: " + user_input);
        textBuffer->place_cursor(textBuffer->end());
        textBuffer->insert_at_cursor("\r\n");
        lock_text(textBuffer->end());
        terminal->scroll_to(inputBegin);

        user_input.append("\r");
        window->hardwareInterface->write(user_input);
    }
}
