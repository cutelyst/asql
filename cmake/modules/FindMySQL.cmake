# SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

# FindMySQL
# ---------
# Finds the MySQL client library (libmysqlclient).
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
# MySQL::MySQL
#
# Result Variables
# ^^^^^^^^^^^^^^^^
# MySQL_FOUND
# MySQL_INCLUDE_DIRS
# MySQL_LIBRARIES
# MySQL_VERSION

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(PC_MySQL QUIET mysqlclient)
endif ()

find_path(MySQL_INCLUDE_DIR
    NAMES mysql/mysql.h
    HINTS
        ${PC_MySQL_INCLUDEDIR}
        ${PC_MySQL_INCLUDE_DIRS}
    PATH_SUFFIXES mysql
)

find_library(MySQL_LIBRARY
    NAMES mysqlclient mysqlclient_r
    HINTS
        ${PC_MySQL_LIBDIR}
        ${PC_MySQL_LIBRARY_DIRS}
)

if (PC_MySQL_VERSION)
    set(MySQL_VERSION ${PC_MySQL_VERSION})
elseif (MySQL_INCLUDE_DIR AND EXISTS "${MySQL_INCLUDE_DIR}/mysql/mysql_version.h")
    file(STRINGS "${MySQL_INCLUDE_DIR}/mysql/mysql_version.h" _mysql_version_line
        REGEX "^#define MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\"")
    if (_mysql_version_line)
        string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" MySQL_VERSION "${_mysql_version_line}")
    endif()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL
    REQUIRED_VARS MySQL_LIBRARY MySQL_INCLUDE_DIR
    VERSION_VAR MySQL_VERSION
)

if (MySQL_FOUND AND NOT TARGET MySQL::MySQL)
    add_library(MySQL::MySQL UNKNOWN IMPORTED)
    set_target_properties(MySQL::MySQL PROPERTIES
        IMPORTED_LOCATION "${MySQL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MySQL_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MySQL_INCLUDE_DIR MySQL_LIBRARY)
