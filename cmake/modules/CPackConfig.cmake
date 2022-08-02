# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

set(CPACK_PACKAGE_VENDOR "Daniel Nicoletti")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Async Sql Qt library.")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CONTACT "Daniel Nicoletti <dantti12@gmail.com>")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR}")

if(UNIX)
    if(NOT CPACK_GENERATOR)
        set(CPACK_GENERATOR "DEB")
    endif()

    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
    endif()
endif()

include(CPack)
