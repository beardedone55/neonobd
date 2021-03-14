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
