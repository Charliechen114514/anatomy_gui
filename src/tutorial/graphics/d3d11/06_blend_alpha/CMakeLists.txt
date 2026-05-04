cmake_minimum_required(VERSION 3.20)
add_executable(06_blend_alpha main.cpp)
target_include_directories(06_blend_alpha PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(06_blend_alpha PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(06_blend_alpha PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 06_blend_alpha POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/06_blend_alpha"
)
if(NOT MSVC)
    target_link_options(06_blend_alpha PRIVATE -municode)
endif()
