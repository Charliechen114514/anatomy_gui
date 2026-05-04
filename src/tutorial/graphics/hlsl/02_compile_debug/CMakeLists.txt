cmake_minimum_required(VERSION 3.20)
add_executable(02_compile_debug main.cpp)
target_include_directories(02_compile_debug PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(02_compile_debug PRIVATE user32 d3d11 dxgi d3dcompiler dxguid)
set_target_properties(02_compile_debug PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 02_compile_debug POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/02_compile_debug"
)
if(NOT MSVC)
    target_link_options(02_compile_debug PRIVATE -municode)
endif()
