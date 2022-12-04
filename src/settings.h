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
#include "combobox.h"

class MainWindow;

class Settings : public Gtk::Box {
    public:
        Settings(MainWindow &window);
        virtual ~Settings();
    protected:
        MainWindow            & main_window;
        Gtk::Box                topRow;
        Gtk::Button             homeButton;
        Gtk::Frame              connectionFrame;
        Gtk::Grid               connectionGrid;
        Gtk::CheckButton        bluetooth_rb,
                                serial_rb;
        Gtk::Grid               serialGrid;
        Gtk::Grid               btGrid;
        Gtk::Label              btHostLabel;
        Gtk::ComboBoxText       btHostCombo;
        Gtk::Label              btDeviceLabel;
        ComboBox<Glib::ustring, Glib::ustring>  btDeviceCombo;
        Gtk::Button             btDeviceScan;
        Gtk::Label              btScanLabel;
        Gtk::ProgressBar        btScanProgress;
        sigc::connection        btScanConnection;
        Glib::RefPtr<Gio::Settings> settings;
        enum interfaceType {
            BLUETOOTH_IF = 0,
            SERIAL_IF = 1
        };
        void on_show() override;
        void selectSerial();
        void selectBluetooth();
        void selectBluetoothController();
        void selectBluetoothDevice();
        void scanBluetooth();
        void scanComplete();
        void updateScanProgress(int percentComplete);
};
