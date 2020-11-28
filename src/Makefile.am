bin_PROGRAMS = obd-scanner-gtk
obd_scanner_gtk_SOURCES = \
	mainwindow.cpp \
	homeview.cpp \
	obd-bluetooth-dbus.cpp \
	obd-scanner-gtkmain.cpp \
	settingsview.cpp \
	resources.c

obd_scanner_gtk_CPPFLAGS = $(GTKMM_CFLAGS) -std=c++17
obd_scanner_gtk_LDFLAGS = $(GTKMM_LIBS)

export PKG_CONFIG_PATH = /usr/local/lib/pkgconfig

resources.c: app.gresource.xml appstyle.css
	glib-compile-resources --target $@ --generate-source $<

clean-local:
	-rm resources.c
