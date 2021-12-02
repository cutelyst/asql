# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

set(asql_SRC
    adatabase.cpp
    adatabase.h
    amigrations.cpp
    apool.cpp
    atransaction.cpp
    adriver.cpp
    adriver.h
    adriverfactory.cpp
    aresult.cpp
    acache.cpp
    apreparedquery.cpp
    apreparedquery.h
)

set(asql_HEADERS
    adatabase.h
    apreparedquery.h
    amigrations.h
    apool.h
    atransaction.h
    aresult.h
    adriver.h
    adriverfactory.h
    acache.h
    aqsqlexports.h
)

set(asql_pg_SRC
    adriverpg.cpp
    adriverpg.h
    apg.cpp
)

set(asql_pg_HEADERS
    apg.h
)

set(asql_HEADERS_PRIVATE
#     common.h
)

add_library(ASqlQt${QT_VERSION_MAJOR}
    ${asql_SRC}
    ${asql_HEADERS}
    ${asql_HEADERS_PRIVATE}
)

#Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(ASqlQt${QT_VERSION_MAJOR}::Core ALIAS ASqlQt${QT_VERSION_MAJOR})

if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(TARGET ASqlQt${QT_VERSION_MAJOR} PROPERTY DEBUG_POSTFIX "d")
endif()

target_compile_definitions(ASqlQt${QT_VERSION_MAJOR}
  PRIVATE
    PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
)

target_include_directories(ASqlQt${QT_VERSION_MAJOR} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/asql-qt${QT_VERSION_MAJOR}/ASql>
)

set_target_properties(ASqlQt${QT_VERSION_MAJOR} PROPERTIES
    EXPORT_NAME Core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_link_libraries(ASqlQt${QT_VERSION_MAJOR}
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
)

## Postgres Driver
add_library(ASqlQt${QT_VERSION_MAJOR}Pg
    ${asql_pg_SRC}
    ${asql_pg_HEADERS}
)

add_library(ASqlQt${QT_VERSION_MAJOR}::Pg ALIAS ASqlQt${QT_VERSION_MAJOR}Pg)

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
        Qt${QT_VERSION_MAJOR}::Core
        ASqlQt${QT_VERSION_MAJOR}::Core
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

## End Postgres Driver

set_property(TARGET ASqlQt${QT_VERSION_MAJOR} PROPERTY PUBLIC_HEADER ${asql_HEADERS})
install(TARGETS ASqlQt${QT_VERSION_MAJOR}
    EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION bin COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT devel
    PUBLIC_HEADER DESTINATION include/asql-qt${QT_VERSION_MAJOR}/ASql COMPONENT devel
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/asql-qt5.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}.pc
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt${QT_VERSION_MAJOR}.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

add_executable(asql-migration${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR} asql_migration.cpp)
target_link_libraries(asql-migration${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR}
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        ASqlQt${QT_VERSION_MAJOR}::Core
        ASqlQt${QT_VERSION_MAJOR}::Pg
)
install(TARGETS asql-migration${PROJECT_VERSION_MAJOR}-qt${QT_VERSION_MAJOR} DESTINATION ${CMAKE_INSTALL_BINDIR})
