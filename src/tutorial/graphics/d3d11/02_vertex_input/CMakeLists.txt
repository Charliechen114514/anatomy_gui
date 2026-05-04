cmake_minimum_required(VERSION 3.20)
add_executable(02_vertex_input main.cpp)
target_include_directories(02_vertex_input PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(02_vertex_input PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(02_vertex_input PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 02_vertex_input POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/02_vertex_input"
)
if(NOT MSVC)
    target_link_options(02_vertex_input PRIVATE -municode)
endif()
