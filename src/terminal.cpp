/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2026  Brian LePage
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
    : m_window{main_window} {
    
    auto& user_interface = m_window->get_ui();
    connect(&m_window.m_view_stack, &QStackedWidget::currentChanged,
            this, &Terminal::on_show);

    m_home_button = user_interface.terminal_home_button;
    connect(m_home_button, &QPushButton::clicked, this,
            &Terminal::home_clicked);

    m_terminal = user_interface.terminal_text_view;
    
    connect(m_terminal, &QPlainTextEdit::cursorPositionChanged,
            this, &Terminal::cursor_moved);

    connect(this, &Terminal::read_data_available,
            this, &Terminal::reader_notification);

    connect(m_terminal, &QPlainTextEdit::textChanged,
            this, text_entered);
}

Terminal::~Terminal() {
    m_stop_reader = true;
    if (m_reader_thread && m_reader_thread->joinable()) {
        m_reader_thread->join();
    }
}

using namespace std::chrono_literals;

void Terminal::read_data() {
    Logger::debug("Terminal reader thread started.");
    auto hwif = m_window->m_hardware_interface;
    // std::operation""ms is included in <chrono> according to c++ docs.
    // NOLINTNEXTLINE(misc-include-cleaner)
    static constexpr std::chrono::milliseconds timeout = 100ms;
    hwif->set_timeout(timeout);
    std::string tempBuffer;
    while (!m_stop_reader) {
        auto bytecount = hwif->read(tempBuffer);
        if (bytecount > 0) {
            const std::lock_guard lock(read_buffer_mutex);
            m_read_buffer.append(tempBuffer);
            emit read_data_available();
        }
    }
    m_reader_stopped = true;
    Logger::debug("Terminal reader thread stopped.");
}

void Terminal::start_reader_thread() {
    if (m_reader_thread && m_reader_thread->joinable()) {
        m_reader_thread->join();
    }

    m_reader_stopped = false;
    m_read_buffer.resize(0);
    m_reader_thread =
        std::make_unique<std::thread>([this]() { this->read_data(); });
}

void Terminal::reader_notification() {
    if (m_reader_stopped) {
        m_reader_thread->join();
        if (!m_stop_reader) {
            start_reader_thread();
        }
        return;
    }

    const std::lock_guard lock(read_buffer_mutex);
    auto pos = m_terminal->textCursor();
    pos.setPosition(m_input_begin);
    m_terminal->setTextCursor(pos);
    m_terminal->insertPlainText(read_buffer);
    m_input_begin = m_terminal->textCursor().position();
    read_buffer.resize(0);
}

void Terminal::on_show() {
    if (m_window->get_view_stack().currentWidget() != 
        m_window->get_ui().terminal_view) {
        return;
    }

    m_terminal->setFocus();
    m_terminal.setPlainText(">");
    m_input_begin = m_terminal->textCursor().position();

    m_stop_reader = false;

    if (!m_reader_stopped) {
        return;
    }

    start_reader_thread();
}

void Terminal::home_clicked() {
    m_stop_reader = true;
    auto home_view = m_window->get_ui().home_view;
    m_window->m_view_stack.setCurrentWidget(home_view);
}

void Terminal::cursor_moved() {
    if (m_terminal->textCursor().position() < m_input_begin) {
        m_terminal->moveCursor(QTextCursor::End);
    }
}

void Terminal::text_entered() {
    auto document = m_terminal->document();
    int last_pos = document->characterCount() - 1;
    if (document->characterAt(last_pos) == '\n' && 
        last_pos > m_input_begin) {
        QTextCursor selection(document);
        selection.setPosition(m_input_begin);
        selection.setPosition(last_pos-1, QTextCursor::KeepAnchor);
        std::string user_input = selection.selectedText();
        Logger::debug("User input: " + user_input);
        m_input_begin = document->characterCount();
        user_input.append("\r");
        m_window->m_hardware_interface->write(user_input);
    }
}
