cmake_minimum_required(VERSION 3.20)
add_executable(03_cbuffer main.cpp)
target_include_directories(03_cbuffer PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(03_cbuffer PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(03_cbuffer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 03_cbuffer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/03_cbuffer"
)
if(NOT MSVC)
    target_link_options(03_cbuffer PRIVATE -municode)
endif()
