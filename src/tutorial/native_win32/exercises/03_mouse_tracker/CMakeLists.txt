cmake_minimum_required(VERSION 3.15)

project(mouse_tracker CXX)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Win32 GUI 应用程序
add_executable(mouse_tracker WIN32
    main.cpp
)

# 链接 Windows 库
target_link_libraries(mouse_tracker
    PRIVATE
    gdi32
    user32
)
