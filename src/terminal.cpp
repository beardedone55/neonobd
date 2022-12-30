/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022  Brian LePage
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

#include "terminal.h"
#include "mainwindow.h"
#include "logger.h"

Terminal::Terminal(MainWindow* window) : 
    window{window},
    visibleView{window->viewStack->property_visible_child_name()}
{
    auto ui = window->ui;
    visibleView.signal_changed().connect(sigc::mem_fun(*this, &Terminal::on_show));

    homeButton = ui->get_widget<Gtk::Button>("terminal_home_button");
    homeButton->signal_clicked().connect(sigc::mem_fun(*this, &Terminal::homeClicked));

    terminal = ui->get_widget<Gtk::TextView>("terminal_text_view");
    textBuffer = Gtk::TextBuffer::create();
    terminal->set_buffer(textBuffer);
    textBuffer->signal_insert().connect(sigc::mem_fun(*this, &Terminal::textEntered));
    tagReadOnly = textBuffer->create_tag();
    tagReadOnly->property_editable() = false;
    inputBegin = textBuffer->create_mark(textBuffer->begin());
    textBuffer->property_cursor_position().signal_changed().connect(
            sigc::mem_fun(*this, &Terminal::cursorMoved));
}


void Terminal::lock_text() {
    textBuffer->apply_tag(tagReadOnly, textBuffer->begin(), textBuffer->end());
    textBuffer->move_mark(inputBegin, textBuffer->end());
}

void Terminal::on_show() {
    if(visibleView.get_value() != "terminal_view")
        return;

    textBuffer->set_text("> ");
    lock_text();
    terminal->grab_focus();
}

void Terminal::homeClicked() {
    //Leaving page, clean up.
    //

    window->viewStack->set_visible_child("home_view");
}

void Terminal::cursorMoved() {
    if(textBuffer->get_insert()->get_iter() < inputBegin->get_iter()) {
        textBuffer->place_cursor(inputBegin->get_iter());
    }
}

void Terminal::textEntered(Gtk::TextBuffer::iterator& pos, const Glib::ustring& text, int bytes) {
    if(text == "\n" && pos > inputBegin->get_iter()) {
        textBuffer->backspace(pos);
        auto user_input = textBuffer->get_text(inputBegin->get_iter(), textBuffer->end());
        Logger::debug("User input: " + user_input);
        textBuffer->place_cursor(textBuffer->end());
        textBuffer->insert_at_cursor("\n> ");
        lock_text();
    }
}
