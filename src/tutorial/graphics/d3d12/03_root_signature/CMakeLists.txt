cmake_minimum_required(VERSION 3.20)
add_executable(03_root_signature main.cpp)
target_include_directories(03_root_signature PRIVATE ${GRAPHICS_COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(03_root_signature PRIVATE user32 d3d12 dxgi d3dcompiler)
set_target_properties(03_root_signature PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET 03_root_signature POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/bin/shaders/03_root_signature"
)
if(NOT MSVC)
    target_link_options(03_root_signature PRIVATE -municode)
endif()
