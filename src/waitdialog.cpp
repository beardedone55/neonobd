// vim: et:ts=4:sw=4

#include "waitdialog.h"

WaitDialog::WaitDialog(const Glib::ustring &name, const Glib::ustring &text) :
    message{text},
    Gtk::Dialog{name, true}
{
    Gtk::Box *content_area = get_content_area();
    
    content_area->append(message);
    content_area->append(progress);    
}

WaitDialog::~WaitDialog() { }


