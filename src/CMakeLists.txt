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
if (DRIVER_POSTGRES)
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

## Sqlite Driver
if (DRIVER_SQLITE)
    set(asql_sqlite_SRC
        sqlite/sqlite3.c
        sqlite/sqlite3.h
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
    )

    set_property(TARGET ASqlQt${QT_VERSION_MAJOR}Sqlite PROPERTY PUBLIC_HEADER ${asql_sqlite_HEADERS})
    install(TARGETS ASqlQt${QT_VERSION_MAJOR}Sqlite
        EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION bin COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT devel
        PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
    )

endif () # End Sqlite Driver

set(TARGET_ASQL_MIGRATION_CMD asql-migration${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR})
add_executable(${TARGET_ASQL_MIGRATION_CMD} asql_migration.cpp)
target_link_libraries(${TARGET_ASQL_MIGRATION_CMD}
    PUBLIC
        Qt::Core
        ASql::Core
        ASql::Migrations
)
if (DRIVER_POSTGRES)
    target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Pg)
    target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_POSTGRES)
endif()
if (DRIVER_SQLITE)
  target_link_libraries(${TARGET_ASQL_MIGRATION_CMD} PRIVATE ASql::Sqlite)
  target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD} PRIVATE DRIVER_SQLITE)
endif()
project_target_compile_definitions(${TARGET_ASQL_MIGRATION_CMD})
install(TARGETS ${TARGET_ASQL_MIGRATION_CMD} DESTINATION ${CMAKE_INSTALL_BINDIR})
