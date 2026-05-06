# 31_file_drop - WM_DROPFILES 文件拖入 ListView

cmake_minimum_required(VERSION 3.20)

add_executable(31_file_drop
    main.cpp
)

target_include_directories(31_file_drop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(31_file_drop PRIVATE
    user32
    gdi32
    comctl32
    shell32
)

set_target_properties(31_file_drop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(31_file_drop PRIVATE -municode)
endif()
