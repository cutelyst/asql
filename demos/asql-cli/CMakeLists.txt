# SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(asql-cli main.cpp)

target_link_libraries(asql-cli
    ASql::Core
    Qt::Core
)

if (ASQL_DRIVER_POSTGRES)
    target_link_libraries(asql-cli ASql::Pg)
    target_compile_definitions(asql-cli PRIVATE ASQL_DRIVER_POSTGRES)
endif()

if (ASQL_DRIVER_SQLITE)
    target_link_libraries(asql-cli ASql::Sqlite)
    target_compile_definitions(asql-cli PRIVATE ASQL_DRIVER_SQLITE)
endif()

if (ASQL_DRIVER_MYSQL)
    target_link_libraries(asql-cli ASql::Mysql)
    target_compile_definitions(asql-cli PRIVATE ASQL_DRIVER_MYSQL)
endif()

if (ASQL_DRIVER_ODBC)
    target_link_libraries(asql-cli ASql::Odbc)
    target_compile_definitions(asql-cli PRIVATE ASQL_DRIVER_ODBC)
endif()
