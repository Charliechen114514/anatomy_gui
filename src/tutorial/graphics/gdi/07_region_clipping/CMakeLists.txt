# 07_region_clipping - GDI Region 裁切

cmake_minimum_required(VERSION 3.20)

add_executable(07_region_clipping main.cpp)

target_include_directories(07_region_clipping PRIVATE
    ${GRAPHICS_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(07_region_clipping PRIVATE user32 gdi32)

set_target_properties(07_region_clipping PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(07_region_clipping PRIVATE -municode)
endif()
