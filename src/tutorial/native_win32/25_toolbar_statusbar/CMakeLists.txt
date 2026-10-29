# 25_toolbar_statusbar - 工具栏 + 状态栏

cmake_minimum_required(VERSION 3.20)

add_executable(25_toolbar_statusbar
    main.cpp
)

target_include_directories(25_toolbar_statusbar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(25_toolbar_statusbar PRIVATE
    user32
    gdi32
    comctl32
)

set_target_properties(25_toolbar_statusbar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(25_toolbar_statusbar PRIVATE -municode)
endif()
