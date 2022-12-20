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
#include "bluetooth-serial-port.h"
#include "combobox.h"

class Settings : public sigc::trackable {
    public:
        Settings(const Glib::RefPtr<Gtk::Builder>& ui, Gtk::Stack* viewStack);
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;
        ~Settings();

        enum InterfaceType {
            BLUETOOTH_IF = 0,
            SERIAL_IF = 1
        };

        InterfaceType getInterfaceType();
        Glib::ustring getSelectedDevice();

    private:
        Glib::RefPtr<Gtk::Builder> ui;
        Gtk::Stack* viewStack;
        Glib::PropertyProxy<Glib::ustring> visibleView;
        std::shared_ptr<BluetoothSerialPort> btHardwareInterface;
        Gtk::Button* homeButton;
        Gtk::CheckButton* bluetooth_rb;
        Gtk::CheckButton* serial_rb;
        Gtk::Label* btHostLabel;
        Gtk::ComboBoxText* btHostCombo;
        Gtk::Label* btDeviceLabel;
        Gtk::Button* btDeviceScan;
        ComboBox<Glib::ustring, Glib::ustring>* btDeviceCombo;
        Gtk::Label* btScanLabel;
        Gtk::ProgressBar* btScanProgress;
        Gtk::Grid* btGrid;
        Gtk::Grid* serialGrid;
        sigc::connection btScanConnection;
        Glib::RefPtr<Gio::Settings> settings;
        InterfaceType iftype;
        void homeClicked();
        void on_show();
        void selectSerial();
        void selectBluetooth();
        void selectBluetoothController();
        void selectBluetoothDevice();
        void scanBluetooth();
        void scanComplete();
        void updateScanProgress(int percentComplete);
};
