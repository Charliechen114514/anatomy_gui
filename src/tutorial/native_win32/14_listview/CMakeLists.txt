# 14_listview - ListView 报表视图

cmake_minimum_required(VERSION 3.20)

add_executable(14_listview
    main.cpp
)

target_include_directories(14_listview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(14_listview PRIVATE
    user32
    gdi32
    comctl32
)

set_target_properties(14_listview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(14_listview PRIVATE -municode)
endif()
