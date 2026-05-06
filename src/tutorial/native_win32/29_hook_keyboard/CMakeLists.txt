# 29_hook_keyboard - 全局低级键盘钩子监听 Print Screen

cmake_minimum_required(VERSION 3.20)

add_executable(29_hook_keyboard
    main.cpp
)

target_include_directories(29_hook_keyboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(29_hook_keyboard PRIVATE
    user32
)

set_target_properties(29_hook_keyboard PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(29_hook_keyboard PRIVATE -municode)
endif()
