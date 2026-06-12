cmake_minimum_required(VERSION 3.20)
project(miniui_stage5 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

add_library(miniui5 STATIC
    src/widget.cpp
    src/layout_strategy.cpp
    src/root_window.cpp
)

target_include_directories(miniui5 PUBLIC include)

target_link_libraries(miniui5 PUBLIC
    ${XCB_LIBRARIES} ${CAIRO_LIBRARIES}
)

target_compile_options(miniui5 PUBLIC
    ${XCB_CFLAGS_OTHER} ${CAIRO_CFLAGS_OTHER}
)

add_subdirectory(example)
