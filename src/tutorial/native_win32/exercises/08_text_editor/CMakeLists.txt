# exercises/08_text_editor - 练习8: 全功能文本编辑器

cmake_minimum_required(VERSION 3.20)

add_executable(08_text_editor
    main.cpp
)

target_include_directories(08_text_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(08_text_editor PRIVATE
    user32
    comctl32
)

set_target_properties(08_text_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT MSVC)
    target_link_options(08_text_editor PRIVATE -municode)
endif()
