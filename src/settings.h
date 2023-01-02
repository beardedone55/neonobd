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
#include <sigc++/connection.h>
#include <glibmm/ustring.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/stack.h>
#include <giomm/settings.h>
#include "bluetooth-serial-port.h"
#include "serial-port.h"
#include "combobox.h"
#include "neonobd_types.h"

class MainWindow;

using neon::InterfaceType;

class Settings : public sigc::trackable {
    public:
        Settings(MainWindow* window);
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;
        ~Settings();

        InterfaceType getInterfaceType();
        Glib::ustring getSelectedDevice();

    private:
        MainWindow* window;
        Glib::PropertyProxy<Glib::ustring> visibleView;
        std::shared_ptr<BluetoothSerialPort> btHardwareInterface;
        std::shared_ptr<SerialPort> serialHardwareInterface;
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
        Gtk::ComboBoxText* serialDeviceCombo;
        Gtk::ComboBoxText* serialBaudrateCombo;
        sigc::connection btScanConnection;
        Glib::RefPtr<Gio::Settings> settings;
        InterfaceType iftype;
        void homeClicked();
        void on_show();
        void selectSerial();
        void selectBluetooth();
        void selectBluetoothController();
        void selectBluetoothDevice();
        void selectSerialDevice();
        void selectSerialBaudrate();
        void scanBluetooth();
        void scanComplete();
        void updateScanProgress(int percentComplete);
        static void populateComboBox(const std::vector<Glib::ustring>& values,
                                     Gtk::ComboBoxText* combobox,
                                     const Glib::ustring& default_value);
};
