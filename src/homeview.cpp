#include "homeview.h"
#include "mainwindow.h"

#include <iostream>

HomeView::BigButton::BigButton(const Glib::ustring &label = "") : Gtk::Button{label}
{
    get_style_context()->add_class("bigbutton");
}

HomeView::BigButton::~BigButton() {}

HomeView::HomeView(MainWindow &window) :
   settings_btn {"⚙️️"},
   connect_btn {"🔃"},
   main_window{window}
{
    set_name("homeview");
    set_row_homogeneous();
    set_column_homogeneous(true);
    set_column_spacing(10);

    main_window.connect_view_button(settings_btn, this, &main_window.settingsView);

    attach(settings_btn, 0, 0);
    attach(connect_btn, 1, 0);
    //show_all_children();
}

HomeView::~HomeView() {}
