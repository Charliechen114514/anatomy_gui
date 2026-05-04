# 08_alpha_blend - GDI Alpha 混合

cmake_minimum_required(VERSION 3.20)

add_executable(08_alpha_blend main.cpp)

target_include_directories(08_alpha_blend PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(08_alpha_blend PRIVATE user32 gdi32 msimg32 comctl32)

set_target_properties(08_alpha_blend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(08_alpha_blend PRIVATE -municode)
endif()
