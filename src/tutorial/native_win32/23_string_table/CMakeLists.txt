# 23_string_table - 字符串表资源

cmake_minimum_required(VERSION 3.20)

add_executable(23_string_table
    main.cpp
    resources.rc
)

target_include_directories(23_string_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(23_string_table PRIVATE
    user32
    gdi32
)

set_target_properties(23_string_table PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(23_string_table PRIVATE -municode)
endif()
