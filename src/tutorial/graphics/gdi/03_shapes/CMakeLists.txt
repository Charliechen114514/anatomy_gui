# 03_shapes - GDI 图形绘制

cmake_minimum_required(VERSION 3.20)

add_executable(03_shapes main.cpp)

target_include_directories(03_shapes PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(03_shapes PRIVATE user32 gdi32)

set_target_properties(03_shapes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(03_shapes PRIVATE -municode)
endif()
