# 06_double_buffer - GDI 双缓冲

cmake_minimum_required(VERSION 3.20)

add_executable(06_double_buffer main.cpp)

target_include_directories(06_double_buffer PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(06_double_buffer PRIVATE user32 gdi32)

set_target_properties(06_double_buffer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(06_double_buffer PRIVATE -municode)
endif()
