cmake_minimum_required(VERSION 3.20)
project(miniui7 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)
find_package(Threads REQUIRED)

add_library(miniui7 STATIC
    src/painter.cpp
    src/widget.cpp
    src/double_buffer.cpp
    src/application.cpp
    src/async_file.cpp
    src/root_window.cpp
    src/label.cpp
    src/button.cpp
)

target_include_directories(miniui7 PUBLIC include)

target_link_libraries(miniui7 PUBLIC
    ${XCB_LIBRARIES}
    ${CAIRO_LIBRARIES}
    Threads::Threads
)

target_compile_options(miniui7 PUBLIC
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

add_subdirectory(example)
