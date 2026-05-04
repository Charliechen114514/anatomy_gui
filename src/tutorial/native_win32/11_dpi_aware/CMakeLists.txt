# 11_dpi_aware - DPI 感知（Per-Monitor V2）

cmake_minimum_required(VERSION 3.20)

add_executable(11_dpi_aware
    main.cpp
)

target_include_directories(11_dpi_aware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(11_dpi_aware PRIVATE
    user32
    gdi32
)

set_target_properties(11_dpi_aware PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(11_dpi_aware PRIVATE -municode)
endif()
