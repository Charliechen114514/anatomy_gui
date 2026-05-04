# 05_bitmap_operations - GDI 位图操作

cmake_minimum_required(VERSION 3.20)

add_executable(05_bitmap_operations main.cpp)

target_include_directories(05_bitmap_operations PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(05_bitmap_operations PRIVATE user32 gdi32)

set_target_properties(05_bitmap_operations PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(05_bitmap_operations PRIVATE -municode)
endif()
