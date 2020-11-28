// vim: ts=4:sw=4:et

#include "mainwindow.h"
#include "homeview.h"
#include "settingsview.h"
#include <iostream>

void MainWindow::showView(Gtk::Widget &view)
{
    remove();
    add(view);
    view.show();
}

void MainWindow::connect_view_button(Gtk::Button &button,
                                     Gtk::Widget *currentView,
                                     Gtk::Widget *nextView)
{
    button.signal_clicked().connect(sigc::bind<Gtk::Widget*, Gtk::Widget*>(
                                    sigc::mem_fun(*this, &MainWindow::switchView),
                                    currentView, nextView));
}

void MainWindow::switchView(Gtk::Widget *oldview, Gtk::Widget *newview)
{
    showView(*newview);
    oldview->hide();
}

MainWindow::MainWindow() :
    homeView{*this},
    settingsView{*this}
{
    set_border_width(10);

    css = Gtk::CssProvider::create();
    css->load_from_resource("/stylesheets/appstyle.css");
    Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(),
                                 css, 
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    set_name("mainwindow");
    showView(homeView);
}

MainWindow::~MainWindow()
{
}

