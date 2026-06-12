cmake_minimum_required(VERSION 3.20)
project(MiniUI_Stage9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

add_library(miniui9 STATIC
    src/widget.cpp
    src/application.cpp
    src/drag_canvas.cpp
)
target_include_directories(miniui9 PUBLIC include)
target_link_libraries(miniui9 PUBLIC ${XCB_LIBRARIES} ${CAIRO_LIBRARIES})
target_compile_options(miniui9 PUBLIC ${XCB_CFLAGS_OTHER} ${CAIRO_CFLAGS_OTHER})

add_subdirectory(example)
