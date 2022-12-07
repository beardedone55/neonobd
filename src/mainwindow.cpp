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
#include "logger.h"
#include <iostream>

void MainWindow::showView(Gtk::Widget &view)
{
    Logger::debug("MainWindow showing view " + Glib::ustring::format(&view));
    unset_child();
    set_child(view);
    view.set_margin(10);
    view.show();
}

void MainWindow::connect_view_button(Gtk::Button &button,
                                     Gtk::Widget *currentView,
                                     Gtk::Widget *nextView)
{
    button.signal_clicked().connect(sigc::bind(
                                    sigc::mem_fun(*this, &MainWindow::switchView),
                                    currentView, nextView));
}

void MainWindow::switchView(Gtk::Widget *oldview, Gtk::Widget *newview)
{
    showView(*newview);
    oldview->hide();
}

MainWindow::MainWindow() :
    home{*this},
    settings{*this},
    bluetoothSerialPort{BluetoothSerialPort::get_instance()}
{
    css = Gtk::CssProvider::create();
    css->load_from_resource("/stylesheets/appstyle.css");
    Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(),
                                 css, 
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    set_name("mainwindow");
    showView(home);
}

MainWindow::~MainWindow()
{
}

