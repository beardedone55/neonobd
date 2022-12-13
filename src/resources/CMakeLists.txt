add_library(resources OBJECT resources.c)

add_custom_command(
    OUTPUT resources.c 
    COMMAND glib-compile-resources --target ${CMAKE_CURRENT_BINARY_DIR}/resources.c --generate-source app.gresource.xml
    DEPENDS app.gresource.xml appstyle.css neonobd_ui.xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(resources PRIVATE ${GTKMM_INCLUDE_DIRS})

install(FILES com.github.beardedone55.neonobd.gschema.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)

find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)

install(CODE "execute_process(COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)")

