cmake_minimum_required(VERSION 3.20)
project(gui_format_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gui_format_test gui_format_test.cpp)
target_include_directories(gui_format_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gui_lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gui_lvgl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
