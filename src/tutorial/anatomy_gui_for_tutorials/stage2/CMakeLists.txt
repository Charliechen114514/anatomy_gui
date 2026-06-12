cmake_minimum_required(VERSION 3.20)

project(AnatomyGUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)

add_library(AnatomyGui SHARED)
target_include_directories(AnatomyGui PUBLIC include)
target_sources(AnatomyGui PUBLIC
    src/application.cpp src/platform/application_xcb.cpp
    src/window_builder.cpp src/window.cpp
    src/platform/window_xcb.cpp src/platform/xcb_helpers.cpp)

add_subdirectory(example)

