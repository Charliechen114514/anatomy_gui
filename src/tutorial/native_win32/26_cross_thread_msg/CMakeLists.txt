# 26_cross_thread_msg - 跨线程 PostMessage 进度条

cmake_minimum_required(VERSION 3.20)

add_executable(26_cross_thread_msg
    main.cpp
)

target_include_directories(26_cross_thread_msg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(26_cross_thread_msg PRIVATE
    comctl32
)

set_target_properties(26_cross_thread_msg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(26_cross_thread_msg PRIVATE -municode)
endif()
