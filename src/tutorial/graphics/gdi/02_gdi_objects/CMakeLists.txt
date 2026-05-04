# 02_gdi_objects - GDI 绘图对象

cmake_minimum_required(VERSION 3.20)

add_executable(02_gdi_objects main.cpp)

target_include_directories(02_gdi_objects PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(02_gdi_objects PRIVATE user32 gdi32)

set_target_properties(02_gdi_objects PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(02_gdi_objects PRIVATE -municode)
endif()
