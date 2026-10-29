# 48_owner_draw - Owner-Draw ListBox 与 ComboBox

cmake_minimum_required(VERSION 3.20)

add_executable(48_owner_draw
    main.cpp
)

target_include_directories(48_owner_draw PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(48_owner_draw PRIVATE
    user32
    gdi32
)

set_target_properties(48_owner_draw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(48_owner_draw PRIVATE -municode)
endif()
