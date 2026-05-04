# 13_wm_notify - WM_NOTIFY 通知机制

cmake_minimum_required(VERSION 3.20)

add_executable(13_wm_notify
    main.cpp
)

target_include_directories(13_wm_notify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(13_wm_notify PRIVATE
    user32
    gdi32
    comctl32
)

set_target_properties(13_wm_notify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(13_wm_notify PRIVATE -municode)
endif()
