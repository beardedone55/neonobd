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

#pragma once

#include <sigc++/trackable.h>
#include <glibmm/ustring.h>
#include <gtkmm/button.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>

class MainWindow;

class Terminal : public sigc::trackable {
    public:
        Terminal(MainWindow* window);

    private:
        MainWindow* window;
        Glib::PropertyProxy<Glib::ustring> visibleView;
        Gtk::Button* homeButton;
        Gtk::TextView* terminal;
        Glib::RefPtr<Gtk::TextBuffer> textBuffer;
        Glib::RefPtr<Gtk::TextBuffer::Tag> tagReadOnly;
        Glib::RefPtr<Gtk::TextBuffer::Mark> inputBegin;

        void on_show();
        void homeClicked();
        void textEntered(Gtk::TextBuffer::iterator& pos, const Glib::ustring& text, int bytes);
        void cursorMoved();
        void lock_text();
};

