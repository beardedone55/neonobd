cmake_minimum_required(VERSION 3.10)

project(neonobd VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

add_executable(neonobd neonobd.cpp bluetooth-serial-port.cpp home.cpp mainwindow.cpp settings.cpp waitdialog.cpp resources.c)

add_custom_command(
    OUTPUT resources.c 
    COMMAND glib-compile-resources --target ${CMAKE_CURRENT_BINARY_DIR}/resources.c --generate-source app.gresource.xml 
    DEPENDS app.gresource.xml appstyle.css
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
   

target_include_directories(neonobd PRIVATE "${PROJECT_SOURCE_DIR}" ${GTKMM_INCLUDE_DIRS})

target_link_libraries(neonobd PRIVATE ${GTKMM_LIBRARIES})

install(TARGETS neonobd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES com.github.beardedone55.neonobd.gschema.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)

find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)

install(CODE "execute_process(COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)")
