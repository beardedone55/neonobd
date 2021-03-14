#ifndef _HOME_VIEW_H_
#define _HOME_VIEW_H_

#include <gtkmm.h>

class MainWindow;

class Home : public Gtk::Grid {
    public:
        Home(MainWindow &window);
        virtual ~Home();
    protected:
        class BigButton : public Gtk::Button
        {
            public:
                BigButton(const Glib::ustring &);
                ~BigButton();

        };
        MainWindow &main_window;
        BigButton settings_btn;
        BigButton connect_btn;

};


#endif
