// vim: et:ts=4:sw=4

#ifndef _WAIT_DIALOG_H_
#define _WAIT_DIALOG_H_

#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <glibmm/ustring.h>

class WaitDialog : Gtk::Dialog
{
    public:
        WaitDialog(Glib::ustring &name = "",
                   Glib::ustring &text = "");

        ~WaitDialog();

    private:
        Gtk::Label message;
        Gtk::ProgressBar progress;
}



#endif
