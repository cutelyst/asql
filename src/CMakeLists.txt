# SPDX-FileCopyrightText: (C) 2020-2024 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

set(asql_SRC
    adatabase.cpp
    adatabase.h
    apool.cpp
    atransaction.cpp

    adriver.cpp
    adriver.h
    adriverfactory.cpp
    aresult.cpp
    acache.cpp
    apreparedquery.cpp
    apreparedquery.h
    acoroexpected.cpp
)

set(asql_HEADERS
    adatabase.h
    apreparedquery.h
    apool.h
    atransaction.h
    acoroexpected.h
    aresult.h
    adriver.h
    adriverfactory.h
    acache.h
)

add_library(ASqlQt${QT_VERSION_MAJOR}
    ${asql_SRC}
    ${asql_HEADERS}
)
generate_export_header(ASqlQt${QT_VERSION_MAJOR}
    BASE_NAME ASQL
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/asql_export.h
    DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
)

#Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(ASql::Core ALIAS ASqlQt${QT_VERSION_MAJOR})
project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR})

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(TARGET ASqlQt${QT_VERSION_MAJOR} PROPERTY DEBUG_POSTFIX "d")
endif()

target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}
    PRIVATE
        PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
)

target_include_directories(ASqlQt${QT_VERSION_MAJOR} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
)

set_target_properties(ASqlQt${QT_VERSION_MAJOR} PROPERTIES
    EXPORT_NAME Core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_link_libraries(ASqlQt${QT_VERSION_MAJOR}
    PUBLIC
        Qt::Core
)

set_property(TARGET ASqlQt${QT_VERSION_MAJOR} PROPERTY PUBLIC_HEADER ${asql_HEADERS})
install(TARGETS ASqlQt${QT_VERSION_MAJOR}
    EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION bin COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT devel
    PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/asql-qt.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

## Migrations Library
set(asql_migrations_SRC
    amigrations.cpp
    amigrations.h
)

set(asql_migrations_HEADERS
    amigrations.h
)

add_library(ASqlQt${QT_VERSION_MAJOR}Migrations
    ${asql_migrations_SRC}
    ${asql_migrations_HEADERS}
)

generate_export_header(ASqlQt${QT_VERSION_MAJOR}Migrations
    BASE_NAME ASQL_MIGRATIONS
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/asql_migrations_export.h
    DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
)

add_library(ASql::Migrations ALIAS ASqlQt${QT_VERSION_MAJOR}Migrations)
project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Migrations)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Migrations PROPERTY DEBUG_POSTFIX "d")
endif()

target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Migrations
    PRIVATE
        PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
)

target_include_directories(ASqlQt${QT_VERSION_MAJOR}Migrations PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
)

set_target_properties(ASqlQt${QT_VERSION_MAJOR}Migrations PROPERTIES
    EXPORT_NAME Migrations
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_link_libraries(ASqlQt${QT_VERSION_MAJOR}Migrations
    PUBLIC
        Qt::Core
        ASql::Core
)

set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Migrations PROPERTY PUBLIC_HEADER ${asql_migrations_HEADERS})
install(TARGETS ASqlQt${QT_VERSION_MAJOR}Migrations
    EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION bin COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT devel
    PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
)
# End Migrations Library

## Postgres Driver
if (ASQL_DRIVER_POSTGRES)
    set(asql_pg_SRC
        adriverpg.cpp
        adriverpg.h
        apg.cpp
    )

    set(asql_pg_HEADERS
        apg.h
    )

    add_library(ASqlQt${QT_VERSION_MAJOR}Pg
        ${asql_pg_SRC}
        ${asql_pg_HEADERS}
    )

    generate_export_header(ASqlQt${QT_VERSION_MAJOR}Pg
        BASE_NAME ASQL_PG
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asql_pg_export.h
        DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
    )

    add_library(ASql::Pg ALIAS ASqlQt${QT_VERSION_MAJOR}Pg)
    project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Pg)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Pg PROPERTY DEBUG_POSTFIX "d")
    endif()

    target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Pg
        PRIVATE
            PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
    )

    target_include_directories(ASqlQt${QT_VERSION_MAJOR}Pg PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
    )

    set_target_properties(ASqlQt${QT_VERSION_MAJOR}Pg PROPERTIES
        EXPORT_NAME Pg
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )

    target_link_libraries(ASqlQt${QT_VERSION_MAJOR}Pg
        PUBLIC
            Qt::Core
            ASql::Core
        PRIVATE
            PostgreSQL::PostgreSQL
    )

    set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Pg PROPERTY PUBLIC_HEADER ${asql_pg_HEADERS})
    install(TARGETS ASqlQt${QT_VERSION_MAJOR}Pg
        EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION bin COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT devel
        PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
    )
endif() # End Postgres Driver

## MySQL Driver
if (DRIVER_MYSQL)
    set(asql_mysql_SRC
        adrivermysql.cpp
        adrivermysql.h
        amysql.cpp
    )

    set(asql_mysql_HEADERS
        amysql.h
    )

    add_library(ASqlQt${QT_VERSION_MAJOR}Mysql
        ${asql_mysql_SRC}
        ${asql_mysql_HEADERS}
    )

    generate_export_header(ASqlQt${QT_VERSION_MAJOR}Mysql
        BASE_NAME ASQL_MYSQL
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asql_mysql_export.h
        DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
    )

    add_library(ASql::Mysql ALIAS ASqlQt${QT_VERSION_MAJOR}Mysql)
    project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Mysql)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Mysql PROPERTY DEBUG_POSTFIX "d")
    endif()

    target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Mysql
        PRIVATE
            PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
    )

    target_include_directories(ASqlQt${QT_VERSION_MAJOR}Mysql PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
    )

    set_target_properties(ASqlQt${QT_VERSION_MAJOR}Mysql PROPERTIES
        EXPORT_NAME Mysql
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )

    target_link_libraries(ASqlQt${QT_VERSION_MAJOR}Mysql
        PUBLIC
            Qt::Core
            ASql::Core
        PRIVATE
            MySQL::MySQL
    )

    set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Mysql PROPERTY PUBLIC_HEADER ${asql_mysql_HEADERS})
    install(TARGETS ASqlQt${QT_VERSION_MAJOR}Mysql
        EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION bin COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT devel
        PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
    )
endif() # End MySQL Driver

## Sqlite Driver
if (ASQL_DRIVER_SQLITE)
    set(asql_sqlite_SRC
        ADriverSqlite.cpp
        ADriverSqlite.hpp
        ASqlite.cpp
    )

    set(asql_sqlite_HEADERS
        ASqlite.hpp
    )

    add_library(ASqlQt${QT_VERSION_MAJOR}Sqlite
        ${asql_sqlite_SRC}
        ${asql_sqlite_HEADERS}
    )

    generate_export_header(ASqlQt${QT_VERSION_MAJOR}Sqlite
        BASE_NAME ASQL_SQLITE
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asql_sqlite_export.h
        DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
    )

    add_library(ASql::Sqlite ALIAS ASqlQt${QT_VERSION_MAJOR}Sqlite)
    project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Sqlite)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Sqlite PROPERTY DEBUG_POSTFIX "d")
    endif()

    target_include_directories(ASqlQt${QT_VERSION_MAJOR}Sqlite PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
    )

    set_target_properties(ASqlQt${QT_VERSION_MAJOR}Sqlite PROPERTIES
        EXPORT_NAME Sqlite
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )

    target_link_libraries(ASqlQt${QT_VERSION_MAJOR}Sqlite
        PUBLIC
            Qt::Core
            ASql::Core
        PRIVATE
            SQLite3::SQLite3
    )

    set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Sqlite PROPERTY PUBLIC_HEADER ${asql_sqlite_HEADERS})
    install(TARGETS ASqlQt${QT_VERSION_MAJOR}Sqlite
        EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION bin COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT devel
        PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
    )

endif () # End Sqlite Driver

## ODBC Driver
if (ASQL_DRIVER_ODBC)
    set(asql_odbc_SRC
        ADriverOdbc.cpp
        ADriverOdbc.hpp
        AOdbc.cpp
    )

    set(asql_odbc_HEADERS
        AOdbc.hpp
    )

    add_library(ASqlQt${QT_VERSION_MAJOR}Odbc
        ${asql_odbc_SRC}
        ${asql_odbc_HEADERS}
    )

    generate_export_header(ASqlQt${QT_VERSION_MAJOR}Odbc
        BASE_NAME ASQL_ODBC
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asql_odbc_export.h
        DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql
    )

    add_library(ASql::Odbc ALIAS ASqlQt${QT_VERSION_MAJOR}Odbc)
    project_target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}Odbc)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Odbc PROPERTY DEBUG_POSTFIX "d")
    endif()

    target_include_directories(ASqlQt${QT_VERSION_MAJOR}Odbc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
    )

    set_target_properties(ASqlQt${QT_VERSION_MAJOR}Odbc PROPERTIES
        EXPORT_NAME Odbc
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )

    target_link_libraries(ASqlQt${QT_VERSION_MAJOR}Odbc
        PUBLIC
            Qt::Core
            ASql::Core
        PRIVATE
            ${ODBC_LIBRARIES}
    )

    set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Odbc PROPERTY PUBLIC_HEADER ${asql_odbc_HEADERS})
    install(TARGETS ASqlQt${QT_VERSION_MAJOR}Odbc
        EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION bin COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT devel
        PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
    )

endif () # End ODBC Driver

if (ASQL_MIGRATION_TOOL)
    set(TARGET_ASQL_MIGRATION_CMD asql-migration${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR})
    add_executable(${TARGET_ASQL_MIGRATION_CMD} asql_migration.cpp)
    target_link_libraries(${TARGET_ASQL_MIGRATION_CMD}
        PUBLIC
            Qt::Core
            ASql::Core
            ASql::Migrations
    )
    if (ASQL_DRIVER_POSTGRES)
        target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Pg)
        target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_POSTGRES)
    endif()
    if (ASQL_DRIVER_SQLITE)
        target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Sqlite)
        target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_SQLITE)
    endif()
    if (ASQL_DRIVER_ODBC)
        target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Odbc)
        target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_ODBC)
    endif()
    if (DRIVER_MYSQL)
        target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Mysql)
        target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_MYSQL)
    endif()
    project_target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD})
    install(TARGETS ${TARGET_ASQL_MIGRATION_CMD} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif () # End Migration Tool
