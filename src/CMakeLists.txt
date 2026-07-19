# This file is part of neonobd - OBD diagnostic software.
# Copyright (C) 2022-2026  Brian LePage
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

cmake_minimum_required(VERSION 3.20)

project(neonobd VERSION 0.1.2)

option(RUN_CPPCHECK "Run cppcheck during compilation" OFF)
option(RUN_CLANG_TIDY "Run clang-tidy during compilation" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wunused -Wcast-align \
                     -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wmisleading-indentation \
                     -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference \
                     -Wformat=2 -Werror")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wunused -Wcast-align \
                     -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wmisleading-indentation \
                     -Wnull-dereference -Wformat=2 -Werror")
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(RUN_CPPCHECK)
    find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck REQUIRED)
    list(APPEND CMAKE_CXX_CPPCHECK --library=qt --enable=all --error-exitcode=1 
        --suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck_suppression_list
         --suppress=unusedFunction --force) 
else()
    unset(CMAKE_CXX_CPPCHECK CACHE)
endif()


find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(neonobd
                  neonobd.cpp
                  hardware-interface.cpp
                  bluetooth-serial-port.cpp
                  home.cpp mainwindow.cpp
                  settings.cpp logger.cpp
                  connect-button.cpp
                  terminal.cpp serial-port.cpp
                  neonobd.ui)

if(RUN_CLANG_TIDY)
    find_program(CMAKE_CXX_CLANG_TIDY NAMES clang-tidy REQUIRED)
else()
    unset(CMAKE_CXX_CLANG_TIDY CACHE)
endif()

target_include_directories(neonobd PRIVATE "${PROJECT_SOURCE_DIR}")

target_link_libraries(neonobd PRIVATE ${SYSTEMD_LIBRARIES} Qt6::Core Qt6::Gui Qt6::Widgets)

install(TARGETS neonobd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging")
    add_subdirectory(packaging)
endif()

