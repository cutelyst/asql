# SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28)
project(libasql VERSION 0.107.0 LANGUAGES CXX)

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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

#
# Options
#
option(ASQL_ENABLE_MAINTAINER_CFLAGS "Enable maintainer CFlags" OFF)
option(ASQL_DRIVER_SQLITE "Enable Sqlite Driver" ON)
if (ASQL_DRIVER_SQLITE)
    find_package(SQLite3)
    if (NOT SQLite3_FOUND)
        set(ASQL_SQLITE3_SUBMODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/sqlite")
        if (EXISTS "${ASQL_SQLITE3_SUBMODULE_DIR}/sqlite3.c")
            message(STATUS "SQLite3 not found via find_package, using bundled submodule")
            enable_language(C)
            add_library(SQLite3_bundled STATIC "${ASQL_SQLITE3_SUBMODULE_DIR}/sqlite3.c")
            target_include_directories(SQLite3_bundled PUBLIC "${ASQL_SQLITE3_SUBMODULE_DIR}")
            add_library(SQLite3::SQLite3 ALIAS SQLite3_bundled)
        else()
            message(FATAL_ERROR
                "SQLite3 not found and 3rdparty/sqlite/sqlite3.c is not available.\n"
                "Either install the SQLite3 development package or provide the amalgamation:\n"
                "  1. Initialize the submodule: git submodule update --init 3rdparty/sqlite\n"
                "  2. Generate the amalgamation (requires tclsh): cd 3rdparty/sqlite && make sqlite3.c\n"
                "     See https://www.sqlite.org/howtocompile.html for details.")
        endif()
    endif()
endif ()

option(ASQL_DRIVER_POSTGRES "Enable PostgreSql Driver" ON)
if (ASQL_DRIVER_POSTGRES)
    find_package(PostgreSQL REQUIRED)
endif ()

option(ASQL_DRIVER_ODBC "Enable unixODBC Driver" ON)
if (ASQL_DRIVER_ODBC)
    find_package(ODBC REQUIRED)
endif ()

# NONE

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
    "/usr" CACHE PATH "asql default install prefix" FORCE)
endif()

set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}" CACHE PATH "Output directory for libraries")

option(BUILD_SHARED_LIBS "Build in shared lib mode" ON)
option(ASQL_BUILD_DEMOS "Build the demos" ON)
option(ASQL_MIGRATION_TOOL "Build the migration tool executable" ON)

#
# Custom C flags
#
if (ASQL_ENABLE_MAINTAINER_CFLAGS)
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
if (ASQL_BUILD_DEMOS)
    add_subdirectory(demos)
endif ()

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(CPackConfig)
