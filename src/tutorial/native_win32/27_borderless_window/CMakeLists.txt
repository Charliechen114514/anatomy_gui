# 27_borderless_window - 无边框可拖拽窗口

cmake_minimum_required(VERSION 3.20)

add_executable(27_borderless_window
    main.cpp
)

target_include_directories(27_borderless_window PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(27_borderless_window PRIVATE
    user32
    gdi32
)

set_target_properties(27_borderless_window PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(27_borderless_window PRIVATE -municode)
endif()
