cmake_minimum_required(VERSION 3.16)
project(EmberInterDebugTool VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Qt5 REQUIRED COMPONENTS Widgets Gui SerialPort Network)

include(FindRapidJSON)

find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
find_package(fmt REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(src/cli)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "Google Test not found, skipping tests")
    endif()
endif()
