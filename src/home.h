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

#include <gtkmm.h>
#include <unordered_set>
#include "connect-button.h"
#include "hardware-interface.h"
#include "neonobd_types.h"

class MainWindow;

using neon::ResponseType;

class Home : public sigc::trackable {
    public:
        Home(MainWindow* window);
        void enable_all();
        void disable_all();
        void enable_button(Gtk::Button* button);
        void disable_button(Gtk::Button* button);
    private:
        MainWindow* window;
        Gtk::Button* settings_btn;
        ConnectButton* connect_btn;
        std::unordered_set<Gtk::Button*> enabled_buttons;

        void settings_clicked();

};

