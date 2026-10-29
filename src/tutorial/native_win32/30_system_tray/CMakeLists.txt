# 30_system_tray - 最小化到托盘 + 右键菜单 + 气泡通知

cmake_minimum_required(VERSION 3.20)

add_executable(30_system_tray
    main.cpp
)

target_include_directories(30_system_tray PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(30_system_tray PRIVATE
    shell32
)

set_target_properties(30_system_tray PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(30_system_tray PRIVATE -municode)
endif()
