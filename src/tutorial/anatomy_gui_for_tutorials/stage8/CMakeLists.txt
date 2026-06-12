cmake_minimum_required(VERSION 3.20)
project(miniui_stage8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)
find_package(Threads REQUIRED)

# ── Static library: miniui8 ──────────────────────────────────────────────────
add_library(miniui8 STATIC
    src/painter.cpp
    src/widget.cpp
    src/application.cpp
    src/double_buffer.cpp
    src/root_window.cpp
    src/text_buffer.cpp
    src/text_canvas.cpp
    src/menu_bar.cpp
    src/status_bar.cpp
    src/editor_view.cpp
    src/editor_presenter.cpp
    src/async_file.cpp
)

target_include_directories(miniui8 PUBLIC include)

target_link_libraries(miniui8 PUBLIC
    ${XCB_LIBRARIES}
    ${CAIRO_LIBRARIES}
    Threads::Threads
)

target_compile_options(miniui8 PUBLIC
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
    -fcoroutines
)

# ── Example application ──────────────────────────────────────────────────────
add_subdirectory(example)
