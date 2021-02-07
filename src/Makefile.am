bin_PROGRAMS = neonobd
neonobd_SOURCES = \
	mainwindow.cpp \
	homeview.cpp \
	obd-bluetooth-dbus.cpp \
	neonobd.cpp \
	settingsview.cpp \
	resources.c

neonobd_CXXFLAGS = -std=c++17
neonobd_CPPFLAGS = $(GTKMM_CFLAGS)
neonobd_LDFLAGS = $(GTKMM_LIBS)

gsettings_SCHEMAS = com.github.beardedone55.neonobd.gschema.xml
@GSETTINGS_RULES@

resources.c: app.gresource.xml appstyle.css
	glib-compile-resources --target $@ --generate-source $<

clean-local:
	-rm resources.c
