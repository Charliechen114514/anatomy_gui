cmake_minimum_required(VERSION 3.20)
add_executable(04_depth_3d main.cpp)
target_include_directories(04_depth_3d PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(04_depth_3d PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(04_depth_3d PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 04_depth_3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/04_depth_3d"
)
if(NOT MSVC)
    target_link_options(04_depth_3d PRIVATE -municode)
endif()
