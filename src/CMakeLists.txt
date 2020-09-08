set(asql_SRC
    adatabase_p.h
    adatabase.cpp
    adatabase.h
    amigrations.cpp
    apool.cpp
    atransaction.cpp
    adriver.cpp
    adriver.h
    aresult.cpp
    acache.cpp
    apreparedquery.cpp
    apreparedquery.h
    adriverpg.cpp
    adriverpg.h
)

set(asql_HEADERS
    adatabase.h
    apreparedquery.h
    amigrations.h
    apool.h
    atransaction.h
    aresult.h
    adriver.h
    acache.h
    aqsqlexports.h
)

set(asql_HEADERS_PRIVATE
#     common.h
)

add_library(ASqlQt5 SHARED
    ${asql_SRC}
    ${asql_HEADERS}
    ${asql_HEADERS_PRIVATE}
)

#Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(ASqlQt5::Core ALIAS ASqlQt5)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(TARGET ASqlQt5 PROPERTY DEBUG_POSTFIX "d")
endif()

target_compile_definitions(ASqlQt5
  PRIVATE
    PLUGINS_PREFER_DEBUG_POSTFIX=$<CONFIG:Debug>
)

target_include_directories(ASqlQt5 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/asql-qt5/ASql>
)

set_target_properties(ASqlQt5 PROPERTIES
    EXPORT_NAME Core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_link_libraries(ASqlQt5
    PUBLIC
        Qt5::Core
    PRIVATE
        PostgreSQL::PostgreSQL
)

set_property(TARGET ASqlQt5 PROPERTY PUBLIC_HEADER ${asql_HEADERS})
install(TARGETS ASqlQt5
    EXPORT ASqlTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION bin COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT devel
    PUBLIC_HEADER DESTINATION include/asql-qt5/ASql COMPONENT devel
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/asql-qt5.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt5.pc
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ASqlQt5.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

add_executable(asql-migration asql_migration.cpp)
target_link_libraries(asql-migration
    PUBLIC
        Qt5::Core
        ASqlQt5::Core
)
