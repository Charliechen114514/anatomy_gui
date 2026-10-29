# 19_dialog_proc - DialogProc 高级模式

cmake_minimum_required(VERSION 3.20)

add_executable(19_dialog_proc
    main.cpp
)

target_include_directories(19_dialog_proc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(19_dialog_proc PRIVATE
    user32
    gdi32
)

set_target_properties(19_dialog_proc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(19_dialog_proc PRIVATE -municode)
endif()
