cmake_minimum_required(VERSION 3.20)
project(miniui_stage10 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -- Dependencies ---------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)
find_package(Threads REQUIRED)

# -- Static library: miniui10 ---------------------------------------------------
add_library(miniui10 STATIC
    src/painter.cpp
    src/widget.cpp
    src/thread_pool.cpp
    src/application.cpp
    src/root_window.cpp
    src/label.cpp
    src/button.cpp
    src/double_buffer.cpp
    src/color_widget.cpp
)

target_include_directories(miniui10 PUBLIC include)

target_link_libraries(miniui10 PUBLIC
    ${XCB_LIBRARIES}
    ${CAIRO_LIBRARIES}
    Threads::Threads
)

target_compile_options(miniui10 PUBLIC
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

# -- Example application --------------------------------------------------------
add_subdirectory(example)
