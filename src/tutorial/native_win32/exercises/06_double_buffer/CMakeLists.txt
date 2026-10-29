cmake_minimum_required(VERSION 3.15)
project(double_buffer CXX)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加 Win32 窗口程序支持
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# 源文件
add_executable(double_buffer
    main.cpp
)

# 链接 Windows 库
if(WIN32)
    target_link_libraries(double_buffer PRIVATE
        gdi32
        user32
    )
endif()

# 设置输出目录
set_target_properties(double_buffer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Windows 特定编译选项
if(MSVC)
    target_compile_options(double_buffer PRIVATE
        /W4
        /utf-8
    )
else()
    target_compile_options(double_buffer PRIVATE
        -Wall
        -Wextra
    )
endif()
