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

if (DRIVER_SQLITE)
    asql_test(sqlite_tst ASql::Sqlite)
endif()
