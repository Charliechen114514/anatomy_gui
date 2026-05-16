cmake_minimum_required(VERSION 3.20)
add_executable(sprite_renderer main.cpp)
target_include_directories(sprite_renderer PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(sprite_renderer PRIVATE user32 d3d11 dxgi d3dcompiler windowscodecs)
set_target_properties(sprite_renderer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET sprite_renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/sprite_renderer"
)
if(NOT MSVC)
    target_link_options(sprite_renderer PRIVATE -municode)
endif()
