# SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)
project(libasql VERSION 0.99.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(GenerateExportHeader)

find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 6.4.0 COMPONENTS Core REQUIRED)

set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Include our cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# Options
#
option(ENABLE_MAINTAINER_CFLAGS "Enable maintainer CFlags" OFF)
option(BUILD_TESTS "Build tests" ON)
option(DRIVER_SQLITE "Enable Sqlite Driver" ON)
if (DRIVER_SQLITE)
    enable_language(C)
endif ()

option(DRIVER_POSTGRES "Enable PostgreSql Driver" ON)
if (DRIVER_POSTGRES)
    find_package(PostgreSQL REQUIRED)
endif ()

# NONE

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
    "/usr" CACHE PATH "asql default install prefix" FORCE)
endif()

set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}" CACHE PATH "Output directory for libraries")

option(BUILD_SHARED_LIBS "Build in shared lib mode" ON)
option(BUILD_DEMOS "Build the demos" ON)

#
# Custom C flags
#
if (ENABLE_MAINTAINER_CFLAGS)
    set (MAINTAINER_CFLAGS "-Werror -Wall -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Wformat -Winit-self")
    add_definitions(${MAINTAINER_CFLAGS}
        -DQT_DEPRECATED_WARNINGS
    )
endif ()

function(project_target_compile_definitions target)
    target_compile_definitions(${target}
        PRIVATE
        QT_NO_KEYWORDS
        QT_NO_CAST_TO_ASCII
        QT_NO_CAST_FROM_ASCII
        QT_STRICT_ITERATORS
        QT_NO_URL_CAST_FROM_STRING
        QT_NO_CAST_FROM_BYTEARRAY
        QT_USE_QSTRINGBUILDER
        QT_NO_SIGNALS_SLOTS_KEYWORDS
        QT_USE_FAST_OPERATOR_PLUS
        QT_DISABLE_DEPRECATED_BEFORE=0x060400
    )
endfunction()

# cmake config files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/asqlqt-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}Config.cmake
    @ONLY
)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/asqlqt-config-version.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}ConfigVersion.cmake
    @ONLY
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ASqlQt${QT_VERSION_MAJOR}/
)

install(EXPORT ASqlTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ASqlQt${QT_VERSION_MAJOR}/
    FILE ASql${PROJECT_VERSION_MAJOR}Qt${QT_VERSION_MAJOR}Targets.cmake
    NAMESPACE ASql::
    COMPONENT Devel
)

add_subdirectory(src)
if (BUILD_DEMOS)
    add_subdirectory(demos)
endif ()

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CPackConfig)
