# 28_subclassing - 子类化 Edit 控件拦截 Enter 键

cmake_minimum_required(VERSION 3.20)

add_executable(28_subclassing
    main.cpp
)

target_include_directories(28_subclassing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(28_subclassing PRIVATE
    comctl32
)

set_target_properties(28_subclassing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(28_subclassing PRIVATE -municode)
endif()
