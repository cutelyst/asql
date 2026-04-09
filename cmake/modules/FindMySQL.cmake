# SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

# FindMySQL
# ---------
# Finds the MySQL/MariaDB client library (libmysqlclient or libmariadb).
#
# Both MySQL 8.0+ and MariaDB Connector/C implement the same non-blocking API
# (mysql_real_query_nonblocking, mysql_store_result_nonblocking, etc.) with a
# compatible ABI. libmariadb-dev ships a mysqlclient.pc compatibility file, so
# on most distributions only one of the two will be installed at a time.
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
    # Prefer the MariaDB Connector/C pkg-config name; fall back to mysqlclient
    # (which MariaDB also provides as a compatibility alias).
    pkg_check_modules(PC_MySQL QUIET libmariadb)
    if (NOT PC_MySQL_FOUND)
        pkg_check_modules(PC_MySQL QUIET mysqlclient)
    endif ()
endif ()

find_path(MySQL_INCLUDE_DIR
    NAMES mysql/mysql.h mysql.h
    HINTS
        ${PC_MySQL_INCLUDEDIR}
        ${PC_MySQL_INCLUDE_DIRS}
    PATH_SUFFIXES mysql mariadb
)

find_library(MySQL_LIBRARY
    NAMES mariadb mysqlclient mysqlclient_r
    HINTS
        ${PC_MySQL_LIBDIR}
        ${PC_MySQL_LIBRARY_DIRS}
)

if (PC_MySQL_VERSION)
    set(MySQL_VERSION ${PC_MySQL_VERSION})
elseif (MySQL_INCLUDE_DIR)
    # MySQL stores version in mysql/mysql_version.h; MariaDB uses mariadb_version.h
    foreach(_version_header
            "${MySQL_INCLUDE_DIR}/mysql/mysql_version.h"
            "${MySQL_INCLUDE_DIR}/mariadb_version.h"
            "${MySQL_INCLUDE_DIR}/mysql_version.h")
        if (EXISTS "${_version_header}")
            file(STRINGS "${_version_header}" _mysql_version_line
                REGEX "^#define (MYSQL|MARIADB)_SERVER_VERSION[ \t]+\"[^\"]+\"")
            if (_mysql_version_line)
                string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" MySQL_VERSION "${_mysql_version_line}")
                break()
            endif()
        endif()
    endforeach()
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
