// vim: et:ts=4:sw=4

#include "waitdialog.h"

WaitDialog::WaitDialog(Glib::ustring &name, Glib::ustring &text) :
    message{text},
    Gtk::Dialog{name, true}
{
    Gtk::Box *content_area = get_content_area();
    
    content_area->add(message);
    content_area->add(progress);    
}

WaitDialog::~WaitDialog() { }


