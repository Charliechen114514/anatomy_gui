# 22_icon_cursor_bitmap - 图标/光标/位图资源

cmake_minimum_required(VERSION 3.20)

add_executable(22_icon_cursor_bitmap
    main.cpp
    resources.rc
)

target_include_directories(22_icon_cursor_bitmap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(22_icon_cursor_bitmap PRIVATE
    user32
    gdi32
)

set_target_properties(22_icon_cursor_bitmap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(22_icon_cursor_bitmap PRIVATE -municode)
endif()
