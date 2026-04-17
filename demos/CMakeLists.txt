# SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

if (ASQL_DRIVER_POSTGRES)
    add_subdirectory(async1)
endif()

if (ASQL_DRIVER_SQLITE)
    add_subdirectory(sqlite)
endif()

if (ASQL_DRIVER_MYSQL)
    add_subdirectory(mysql)
endif()

if (ASQL_DRIVER_POSTGRES OR ASQL_DRIVER_SQLITE OR ASQL_DRIVER_MYSQL OR ASQL_DRIVER_ODBC)
    add_subdirectory(asql-cli)
endif()
