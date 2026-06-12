cmake_minimum_required(VERSION 3.20)
project(miniui_stage6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

# ── Static library: miniui6 ──────────────────────────────────────────────────
add_library(miniui6 STATIC
    src/painter.cpp
    src/widget.cpp
    src/root_window.cpp
    src/label.cpp
    src/button.cpp
    src/color_widget.cpp
)

target_include_directories(miniui6 PUBLIC include)

target_link_libraries(miniui6 PUBLIC
    ${XCB_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

target_compile_options(miniui6 PRIVATE
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

# ── Example application ──────────────────────────────────────────────────────
add_subdirectory(example)
