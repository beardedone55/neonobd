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

Home::Home(const Glib::RefPtr<Gtk::Builder>& ui, Gtk::Stack* viewStack) : 
    viewStack{viewStack}
{
    //Settings button
    settings_btn = ui->get_widget<Gtk::Button>("settings_button");
    auto settings_clicked = [this](){this->viewStack->set_visible_child("settings_view");};
    settings_btn->signal_clicked().connect(settings_clicked);

    //Connect button
    connect_btn = ui->get_widget<Gtk::Button>("connect_button");
}
