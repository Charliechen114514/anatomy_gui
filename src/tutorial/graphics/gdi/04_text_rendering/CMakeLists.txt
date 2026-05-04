# 04_text_rendering - GDI 文字渲染

cmake_minimum_required(VERSION 3.20)

add_executable(04_text_rendering main.cpp)

target_include_directories(04_text_rendering PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(04_text_rendering PRIVATE user32 gdi32)

set_target_properties(04_text_rendering PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(04_text_rendering PRIVATE -municode)
endif()
