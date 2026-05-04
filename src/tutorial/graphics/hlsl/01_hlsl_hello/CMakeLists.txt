cmake_minimum_required(VERSION 3.20)
add_executable(01_hlsl_hello main.cpp)
target_include_directories(01_hlsl_hello PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(01_hlsl_hello PRIVATE user32 d3d11 dxgi d3dcompiler)
set_target_properties(01_hlsl_hello PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 01_hlsl_hello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/01_hlsl_hello"
)
if(NOT MSVC)
    target_link_options(01_hlsl_hello PRIVATE -municode)
endif()
