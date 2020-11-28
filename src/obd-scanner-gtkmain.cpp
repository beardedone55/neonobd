/* vim:ts=3:sw=3:et
*/

#include "mainwindow.h"
#include <gtkmm/application.h>

int main(int argc, char *argv[]) {

   auto app = Gtk::Application::create(argc, argv, "com.github.beardedone.obd-scanner");

   MainWindow window;

   return app->run(window);

}
