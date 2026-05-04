# 21_menu_resource - 菜单资源

cmake_minimum_required(VERSION 3.20)

add_executable(21_menu_resource
    main.cpp
    resources.rc
)

target_include_directories(21_menu_resource PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(21_menu_resource PRIVATE
    user32
    gdi32
)

set_target_properties(21_menu_resource PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(21_menu_resource PRIVATE -municode)
endif()
