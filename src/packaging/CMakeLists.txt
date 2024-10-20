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

if(UNIX)
    if(NOT APPLE AND NOT CYGWIN)
        option(PACKAGE_RPM "Create RPM Package" OFF)
        option(PACKAGE_DEB "Create DEB Package" OFF)
        option(PACKAGE_ARCH "Create ARCH Package" OFF)
    endif()
endif()

set(CPACK_PACKAGE_DESCRIPTION "neonobd OBD diagnostic software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "neonobd")
set(CPACK_SOURCE_IGNORE_FILES "/packaging")
set(CPACK_PACKAGE_CONTACT "Brian LePage")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
list(APPEND CPACK_CUSTOM_INSTALL_VARIABLES SKIP_SCHEMA_COMPILATION=1)

if(PACKAGE_RPM)
    set(POST_INST_PREFIX "%{_prefix}")
    configure_file(postinst.in postinst.sh @ONLY)
    list(APPEND CPACK_GENERATOR RPM)
    list(APPEND CPACK_SOURCE_GENERATOR RPM)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL 3.0")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/postinst.sh")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

if(PACKAGE_DEB)
    set(POST_INST_PREFIX "${CPACK_PACKAGING_INSTALL_PREFIX}")
    configure_file(postinst.in postinst @ONLY)
    list(APPEND CPACK_GENERATOR DEB)
    list(APPEND CPACK_SOURCE_GENERATOR DEB)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/postinst")
    list(APPEND CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${GTKMM_LIBRARIES}")
endif()

if(PACKAGE_ARCH)
    #Arch Binary Package:
    list(APPEND CPACK_GENERATOR External)
    list(APPEND CPACK_SOURCE_GENERATOR External)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/create_arch_pkg.cmake")
endif()

set(SAVED_CPACK_GENERATOR "${CPACK_GENERATOR}")
set(SAVED_CPACK_SOURCE_GENERATOR "${CPACK_SOURCE_GENERATOR}")

include(CPack)

# If no package generators are selected, CPACK will automatically select
# a set of generators to use.  The following will override that behavior
# if the user selected no packages by removing the generators in the CPACK
# config files.

macro(remove_generators CONFIG_FILE)
    file(READ "${CONFIG_FILE}" FILE_DATA)
    string(REGEX REPLACE "CPACK_GENERATOR \"[^\"]*\"" "CPACK_GENERATOR \"\"" FILE_DATA "${FILE_DATA}")
    file(WRITE "${CONFIG_FILE}" "${FILE_DATA}")
endmacro()

if(NOT SAVED_CPACK_GENERATOR)
    remove_generators("${CPACK_OUTPUT_CONFIG_FILE}")
endif()

if(NOT SAVED_CPACK_SOURCE_GENERATOR)
    remove_generators("${CPACK_SOURCE_OUTPUT_CONFIG_FILE}")
endif()

