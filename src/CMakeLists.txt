# This file is part of neonobd - OBD diagnostic software.
# Copyright (C) 2022-2023  Brian LePage
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

project(neonobd VERSION 0.1)

option(RUN_CPPCHECK "Run cppcheck during compilation" OFF)
option(RUN_CLANG_TIDY "Run clang-tidy during compilation" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wunused -Wcast-align \
                     -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wmisleading-indentation \
                     -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference \
                     -Wformat=2")

if(RUN_CPPCHECK)
    find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck REQUIRED)
    list(APPEND CMAKE_CXX_CPPCHECK --enable=all --error-exitcode=1 
        --suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck_suppression_list
         --suppress=unusedFunction --force) 
else()
    unset(CMAKE_CXX_CPPCHECK CACHE)
endif()

if(RUN_CLANG_TIDY)
    find_program(CMAKE_CXX_CLANG_TIDY NAMES clang-tidy REQUIRED)
else()
    unset(CMAKE_CXX_CLANG_TIDY CACHE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

add_executable(neonobd neonobd.cpp hardware-interface.cpp bluetooth-serial-port.cpp home.cpp mainwindow.cpp settings.cpp logger.cpp connect-button.cpp terminal.cpp serial-port.cpp elm327.cpp obd.cpp dropdown.cpp)

target_include_directories(neonobd SYSTEM PRIVATE ${GTKMM_INCLUDE_DIRS})
target_include_directories(neonobd PRIVATE "${PROJECT_SOURCE_DIR}")

target_link_libraries(neonobd PRIVATE ${GTKMM_LIBRARIES} resources)

install(TARGETS neonobd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


add_subdirectory(resources)
