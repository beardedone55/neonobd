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

find_program(MAKEPKG makepkg REQUIRED)
list(GET CPACK_BUILD_SOURCE_DIRS 0 SOURCE_DIRECTORY)
string(CONCAT ARCH_PACKAGE_NAME 
       "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}"
       ".${CPACK_PACKAGE_VERSION_MINOR}-${CPACK_PACKAGE_VERSION_PATCH}")

set(ARCH_PKG "${CPACK_PACKAGE_DIRECTORY}/packaging")

find_program(UNAME uname REQURIED)
execute_process(COMMAND "${UNAME}" -m OUTPUT_VARIABLE 
                ARCH_PACKAGE_ARCHITECTURE
                OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CPACK_TOPLEVEL_TAG STREQUAL "Linux")
    message("Creating Arch Package......")

    set(ARCH_BUILD_DIR "${CPACK_PACKAGE_DIRECTORY}")
    configure_file("${SOURCE_DIRECTORY}/packaging/PKGBUILD.in" "${ARCH_PKG}/PKGBUILD" @ONLY)

    execute_process(COMMAND "${MAKEPKG}" -f 
                    WORKING_DIRECTORY "${ARCH_PKG}")

    string(CONCAT ARCH_PACKAGE_NAME "${ARCH_PACKAGE_NAME}"
           "-${ARCH_PACKAGE_ARCHITECTURE}.pkg.tar.zst")

    list(APPEND CPACK_EXTERNAL_BUILT_PACKAGES 
         "${ARCH_PKG}/${ARCH_PACKAGE_NAME}")

elseif(CPACK_TOPLEVEL_TAG STREQUAL "Linux-Source")
    message("Creating Arch Source Package......")
    set(ARCH_PACKAGE_SOURCES ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz)
    set(ARCH_BUILD_DIR "./build")
    configure_file("${SOURCE_DIRECTORY}/packaging/PKGBUILD.in" "${ARCH_PKG}/PKGBUILD" @ONLY)


    # Create exclusion list for source tarball.
    # CPACK_SOURCE_IGNORE_FILES contains list of REGEX
    # for files that should not be included.
    set(EXCLUDE_FILE "${ARCH_PKG}/src_excludes")
    file(REMOVE "${EXCLUDE_FILE}")
    file(GLOB_RECURSE SRC_FILES RELATIVE "${SOURCE_DIRECTORY}/.."
         LIST_DIRECTORIES true  "${SOURCE_DIRECTORY}/*")
    foreach(SRC_FILE ${SRC_FILES})
        foreach(EXCLUDE_PATTERN ${CPACK_SOURCE_IGNORE_FILES})
            if(SRC_FILE MATCHES "${EXCLUDE_PATTERN}")
               file(APPEND "${EXCLUDE_FILE}" "${SRC_FILE}\n")
            endif()
        endforeach()
    endforeach()

    find_program(TAR tar REQUIRED)
    set(TAR_COMMAND ${TAR} "--exclude-from=${EXCLUDE_FILE}" -czf
        "${ARCH_PKG}/${ARCH_PACKAGE_SOURCES}" src)

    message("${TAR_COMMAND}")

    execute_process(COMMAND "${TAR_COMMAND}" 
                    WORKING_DIRECTORY "${SOURCE_DIRECTORY}/..")

    execute_process(COMMAND "${MAKEPKG}" -g
                    OUTPUT_VARIABLE SOURCE_CHECKSUM
                    WORKING_DIRECTORY "${ARCH_PKG}")

    file(APPEND "${ARCH_PKG}/PKGBUILD" "${SOURCE_CHECKSUM}")

    execute_process(COMMAND "${MAKEPKG}" -f -S 
                    WORKING_DIRECTORY "${ARCH_PKG}")

    string(CONCAT ARCH_PACKAGE_NAME "${ARCH_PACKAGE_NAME}"
           ".src.tar.gz")

    list(APPEND CPACK_EXTERNAL_BUILT_PACKAGES 
         "${ARCH_PKG}/${ARCH_PACKAGE_NAME}")

endif()

