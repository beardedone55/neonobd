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

option(INTERACTIVE_TESTS "Enable interactive unit tests" OFF)

add_executable(dbus-type-test
               dbus-type-test.cpp)

target_include_directories(dbus-type-test PRIVATE "${PROJECT_SOURCE_DIR}")

target_link_libraries(dbus-type-test PRIVATE ${SYSTEMD_LIBRARIES})
           
add_test(NAME DBusTypeTest COMMAND dbus-type-test)

add_executable(event-handler-test
               event-handler-test.cpp
               ${PROJECT_SOURCE_DIR}/event-handler.cpp)
           
target_include_directories(event-handler-test PRIVATE "${PROJECT_SOURCE_DIR}")

target_link_libraries(event-handler-test PRIVATE ${SYSTEMD_LIBRARIES})

add_test(NAME EventHandlerTest COMMAND event-handler-test 40)

if(CPPCHECK_BIN)
    set_target_properties(dbus-type-test PROPERTIES
        CXX_CPPCHECK "${CPPCHECK_BIN}")
    set_target_properties(event-handler-test PROPERTIES
        CXX_CPPCHECK "${CPPCHECK_BIN};--suppress=unusedFunction")
endif()

if(INTERACTIVE_TESTS)
    add_executable(btsp-test
                   btsp-test.cpp
                   ${PROJECT_SOURCE_DIR}/hardware-interface.cpp
                   ${PROJECT_SOURCE_DIR}/event-handler.cpp
                   ${PROJECT_SOURCE_DIR}/bluetooth-serial-port.cpp)

    target_include_directories(btsp-test PRIVATE "${PROJECT_SOURCE_DIR}")

    target_link_libraries(btsp-test PRIVATE ${SYSTEMD_LIBRARIES})

    add_executable(serial-port-test
                   serial-port-test.cpp
                   ${PROJECT_SOURCE_DIR}/hardware-interface.cpp
                   ${PROJECT_SOURCE_DIR}/event-handler.cpp
                   ${PROJECT_SOURCE_DIR}/serial-port.cpp)

    target_include_directories(serial-port-test PRIVATE "${PROJECT_SOURCE_DIR}")

    target_link_libraries(serial-port-test PRIVATE ${SYSTEMD_LIBRARIES})
    if(CPPCHECK_BIN)
        set_target_properties(btsp-test serial-port-test PROPERTIES
            CXX_CPPCHECK "${CPPCHECK_BIN};--inline-suppr;--suppress=unusedFunction")
    endif()
endif()

