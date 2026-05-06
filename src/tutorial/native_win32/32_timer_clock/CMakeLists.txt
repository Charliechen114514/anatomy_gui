# 32_timer_clock - WM_TIMER 模拟时钟

cmake_minimum_required(VERSION 3.20)

add_executable(32_timer_clock
    main.cpp
)

target_include_directories(32_timer_clock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(32_timer_clock PRIVATE
    user32
    gdi32
)

set_target_properties(32_timer_clock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(32_timer_clock PRIVATE -municode)
endif()
