cmake_minimum_required(VERSION 3.20)
project(hands_on_cairo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

add_executable(hands_on_cairo main.cpp)
target_include_directories(hands_on_cairo PRIVATE
    ${XCB_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})
target_link_libraries(hands_on_cairo PRIVATE
    ${XCB_LIBRARIES} ${CAIRO_LIBRARIES})
target_compile_options(hands_on_cairo PRIVATE
    ${XCB_CFLAGS_OTHER} ${CAIRO_CFLAGS_OTHER})
