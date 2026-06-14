# 52_advanced_messages - 高级输入消息：触控画板
#   WM_TOUCH / WM_WINDOWPOSCHANGING / WM_PRINTCLIENT / WM_DISPLAYCHANGE

cmake_minimum_required(VERSION 3.20)

add_executable(52_advanced_messages
    main.cpp
)

target_include_directories(52_advanced_messages PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(52_advanced_messages PRIVATE
    user32
    gdi32
)

set_target_properties(52_advanced_messages PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(52_advanced_messages PRIVATE -municode)
endif()
