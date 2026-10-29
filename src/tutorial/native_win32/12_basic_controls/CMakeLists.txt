# 12_basic_controls - 基础控件（Button/Edit/ListBox/ComboBox）

cmake_minimum_required(VERSION 3.20)

add_executable(12_basic_controls
    main.cpp
)

target_include_directories(12_basic_controls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(12_basic_controls PRIVATE
    user32
    gdi32
)

set_target_properties(12_basic_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(12_basic_controls PRIVATE -municode)
endif()
