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

#include "mainwindow.h"

Home::BigButton::BigButton(const Glib::ustring &label = "") : Gtk::Button{label}
{
    get_style_context()->add_class("bigbutton");
}

Home::BigButton::~BigButton() {}

Home::Home(MainWindow &window) :
   settings_btn {"⚙️️"},
   connect_btn {"🔃"},
   main_window{window}
{
    set_name("homeview");
    set_row_homogeneous();
    set_column_homogeneous(true);
    set_column_spacing(10);

    main_window.connect_view_button(settings_btn, this, &main_window.settings);

    attach(settings_btn, 0, 0);
    attach(connect_btn, 1, 0);
    //show_all_children();
}

Home::~Home() {}
