#ifndef _SETTINGS_VIEW_H_
#define _SETTINGS_VIEW_H_

#include <gtkmm.h>

class MainWindow;

class SettingsView : public Gtk::VBox {
    public:
        SettingsView(MainWindow &window);
        virtual ~SettingsView();
    protected:
        MainWindow            & main_window;
        Gtk::ButtonBox          topRow;
        Gtk::Button             homeButton;
        Gtk::Frame              connectionFrame;
        Gtk::Grid               connectionGrid;
        Gtk::RadioButton::Group connectionTypes;
        Gtk::RadioButton        bluetooth_rb,
                                serial_rb;
        Gtk::Grid               serialGrid;
        Gtk::Grid               btGrid;
        Gtk::Label              btHostLabel;
        Gtk::ComboBoxText       btHostCombo;
        Gtk::Label              btDeviceLabel;
        Gtk::ComboBoxText       btDeviceCombo;
        Gtk::Button             btDeviceScan;
        Gtk::Label              btScanLabel;
        Gtk::ProgressBar        btScanProgress;
        sigc::connection        btScanConnection;
        void on_show() override;
        void selectSerial();
        void selectBluetooth();
        void scanBluetooth();
        void scanComplete();
        void updateScanProgress(int percentComplete);
};

#endif
