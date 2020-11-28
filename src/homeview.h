#ifndef _HOME_VIEW_H_
#define _HOME_VIEW_H_

#include <gtkmm.h>

class MainWindow;

class HomeView : public Gtk::Grid {
    public:
        HomeView(MainWindow &window);
        virtual ~HomeView();
    protected:
        class BigButton : public Gtk::Button
        {
            public:
                BigButton(const Glib::ustring &);
                ~BigButton();

        };
        MainWindow &main_window;
        BigButton settings_btn;

};


#endif
