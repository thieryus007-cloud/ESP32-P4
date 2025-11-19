cmake_minimum_required(VERSION 3.20)
project(gui_format_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gui_lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gui_lvgl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/event_types
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_executable(gui_format_test gui_format_test.cpp)
target_include_directories(gui_format_test PRIVATE ${TEST_INCLUDE_DIRS})

add_executable(pack_stats_utils_test
    pack_stats_utils_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gui_lvgl/pack_stats_utils.cpp)
target_include_directories(pack_stats_utils_test PRIVATE ${TEST_INCLUDE_DIRS})
