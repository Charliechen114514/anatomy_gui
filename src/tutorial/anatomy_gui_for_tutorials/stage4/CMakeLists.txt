cmake_minimum_required(VERSION 3.20)
project(miniui_stage4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

# ── Static library: miniui4 ──────────────────────────────────
add_library(miniui4 STATIC
    src/widget.cpp
    src/root_window.cpp
    src/label.cpp
    src/color_rect.cpp
)

target_include_directories(miniui4 PUBLIC include)

target_link_libraries(miniui4 PUBLIC
    ${XCB_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

target_compile_options(miniui4 PUBLIC
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

# ── Example application ──────────────────────────────────────
add_subdirectory(example)
