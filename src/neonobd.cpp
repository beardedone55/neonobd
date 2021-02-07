/* vim:ts=3:sw=3:et
*/

#include "mainwindow.h"
#include <gtkmm/application.h>

int main(int argc, char *argv[]) {

   auto app = Gtk::Application::create("com.github.beardedone55.neonobd");

   return app->make_window_and_run<MainWindow>(argc, argv);
}
