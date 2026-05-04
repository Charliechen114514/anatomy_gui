# 17_modal_dialog - 模态对话框

cmake_minimum_required(VERSION 3.20)

add_executable(17_modal_dialog
    main.cpp
)

target_include_directories(17_modal_dialog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(17_modal_dialog PRIVATE
    user32
    gdi32
)

set_target_properties(17_modal_dialog PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(17_modal_dialog PRIVATE -municode)
endif()
