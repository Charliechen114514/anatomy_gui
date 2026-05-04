# 24_dialog_template - 对话框模板资源

cmake_minimum_required(VERSION 3.20)

add_executable(24_dialog_template
    main.cpp
    resources.rc
)

target_include_directories(24_dialog_template PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(24_dialog_template PRIVATE
    user32
    gdi32
)

set_target_properties(24_dialog_template PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(24_dialog_template PRIVATE -municode)
endif()
