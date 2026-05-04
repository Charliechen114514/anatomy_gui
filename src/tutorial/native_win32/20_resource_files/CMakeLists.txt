# 20_resource_files - .rc 资源文件基础

cmake_minimum_required(VERSION 3.20)

add_executable(20_resource_files
    main.cpp
    resources.rc
)

target_include_directories(20_resource_files PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(20_resource_files PRIVATE
    user32
    gdi32
)

set_target_properties(20_resource_files PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(20_resource_files PRIVATE -municode)
endif()
