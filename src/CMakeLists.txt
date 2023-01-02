# This file is part of neonobd - OBD diagnostic software.
# Copyright (C) 2022  Brian LePage
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.10)

project(neonobd VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

add_executable(neonobd neonobd.cpp hardware-interface.cpp bluetooth-serial-port.cpp home.cpp mainwindow.cpp settings.cpp waitdialog.cpp logger.cpp connect-button.cpp terminal.cpp serial-port.cpp)

target_include_directories(neonobd PRIVATE "${PROJECT_SOURCE_DIR}" ${GTKMM_INCLUDE_DIRS})

target_link_libraries(neonobd PRIVATE ${GTKMM_LIBRARIES} resources)

install(TARGETS neonobd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(resources)
