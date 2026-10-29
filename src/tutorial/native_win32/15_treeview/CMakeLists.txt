# 15_treeview - TreeView 树形控件

cmake_minimum_required(VERSION 3.20)

add_executable(15_treeview
    main.cpp
)

target_include_directories(15_treeview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(15_treeview PRIVATE
    user32
    gdi32
    comctl32
)

set_target_properties(15_treeview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(15_treeview PRIVATE -municode)
endif()
