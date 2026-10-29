# 16_more_controls - 更多控件（ProgressBar/TrackBar/UpDown/Tab）

cmake_minimum_required(VERSION 3.20)

add_executable(16_more_controls
    main.cpp
)

target_include_directories(16_more_controls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(16_more_controls PRIVATE
    user32
    gdi32
    comctl32
)

set_target_properties(16_more_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(16_more_controls PRIVATE -municode)
endif()
