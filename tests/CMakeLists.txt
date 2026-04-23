project(asql_tests
    LANGUAGES CXX
)

find_package(QT NAMES Qt6 COMPONENTS Core Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 6.4.0 COMPONENTS Core Test REQUIRED)

set(CMAKE_AUTOMOC ON)

add_library(coverage_test
    OBJECT
        CoverageObject.cpp
        CoverageObject.hpp
)
target_link_libraries(coverage_test PUBLIC Qt::Test ASql::Core)
target_include_directories(coverage_test PUBLIC "${PROJECT_SOURCE_DIR}/../src")

function(asql_test _testname _driver)
    add_executable(${_testname} ${_testname}.cpp)
    add_test(NAME ${_testname} COMMAND ${_testname})
    target_link_libraries(${_testname} PUBLIC ${_driver} coverage_test)
endfunction()

# Shared OBJECT library with type round-trip test implementations.
# At least one driver must be available for it to be useful; guard accordingly.
if (ASQL_DRIVER_SQLITE OR ASQL_DRIVER_POSTGRES OR ASQL_DRIVER_MYSQL OR ASQL_DRIVER_ODBC)
    add_library(types_test_common
        OBJECT
            tst_types_common.h
            tst_types_common.cpp
    )
    target_link_libraries(types_test_common PUBLIC coverage_test)

    add_library(prepared_test_common
        OBJECT
            tst_prepared_common.h
            tst_prepared_common.cpp
    )
    target_link_libraries(prepared_test_common PUBLIC coverage_test)
endif()

function(asql_types_test _testname _driver)
    add_executable(${_testname} ${_testname}.cpp)
    add_test(NAME ${_testname} COMMAND ${_testname})
    target_link_libraries(${_testname} PUBLIC ${_driver} coverage_test types_test_common)
endfunction()

function(asql_prepared_test _testname _driver)
    add_executable(${_testname} ${_testname}.cpp)
    add_test(NAME ${_testname} COMMAND ${_testname})
    target_link_libraries(${_testname} PUBLIC ${_driver} coverage_test prepared_test_common)
endfunction()

if (ASQL_DRIVER_SQLITE)
    asql_test(sqlite_tst ASql::Sqlite)
    asql_types_test(tst_TypesSqlite ASql::Sqlite)
    asql_prepared_test(tst_PreparedSqlite ASql::Sqlite)
endif()

if (ASQL_DRIVER_POSTGRES)
    asql_types_test(tst_TypesPostgres ASql::Pg)
    asql_prepared_test(tst_PreparedPostgres ASql::Pg)
endif()

if (ASQL_DRIVER_MYSQL)
    asql_types_test(tst_TypesMysql ASql::Mysql)
    asql_prepared_test(tst_PreparedMysql ASql::Mysql)
endif()

if (ASQL_DRIVER_ODBC)
    asql_types_test(tst_TypesOdbc ASql::Odbc)
    asql_prepared_test(tst_PreparedOdbc ASql::Odbc)
endif()
