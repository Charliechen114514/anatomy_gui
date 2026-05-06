# exercises/07_cross_thread_postmsg - 练习7: 跨线程 PostMessage 进度条

cmake_minimum_required(VERSION 3.20)

add_executable(07_cross_thread_postmsg
    main.cpp
)

target_include_directories(07_cross_thread_postmsg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(07_cross_thread_postmsg PRIVATE
    user32
    comctl32
)

set_target_properties(07_cross_thread_postmsg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(07_cross_thread_postmsg PRIVATE -municode)
endif()
