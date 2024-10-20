# This file is part of neonobd - OBD diagnostic software.
# Copyright (C) 2024  Brian LePage
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

if(NOT DEFINED SKIP_SCHEMA_COMPILATION)
    find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
    execute_process(COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)
else()
    message("Skipping schema compilation.")
endif()

