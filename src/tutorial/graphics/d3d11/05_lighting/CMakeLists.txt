cmake_minimum_required(VERSION 3.20)
add_executable(05_lighting main.cpp)
target_include_directories(05_lighting PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(05_lighting PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(05_lighting PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 05_lighting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/05_lighting"
)
if(NOT MSVC)
    target_link_options(05_lighting PRIVATE -municode)
endif()
