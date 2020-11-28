// vim: ts=4:sw=4:et

#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_

#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>

#include "homeview.h"
#include "settingsview.h"
#include "obd-bluetooth-dbus.h"

class MainWindow : public Gtk::Window
{
    public:
        MainWindow();
        virtual ~MainWindow();
        void connect_view_button(Gtk::Button &button, 
                                 Gtk::Widget *currentView,
                                 Gtk::Widget *nextView);
        BlueTooth bluetooth;
        HomeView homeView;
        SettingsView settingsView;
    protected:
        Glib::RefPtr<Gtk::CssProvider> css;
        void switchView(Gtk::Widget *oldview, Gtk::Widget *newview);
        void showView(Gtk::Widget &view);
};

#endif
