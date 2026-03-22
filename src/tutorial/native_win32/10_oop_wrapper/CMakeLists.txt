# 10_oop_wrapper - 面向对象封装示例

cmake_minimum_required(VERSION 3.20)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(10_oop_wrapper
    main.cpp
)

target_include_directories(10_oop_wrapper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接 Windows GUI 库
target_link_libraries(10_oop_wrapper PRIVATE
    user32
    gdi32
)

# 设置输出目录
set_target_properties(10_oop_wrapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# MinGW: 使用 -municode 支持 wWinMain 入口点
if(NOT MSVC)
    target_link_options(10_oop_wrapper PRIVATE -municode)
endif()
