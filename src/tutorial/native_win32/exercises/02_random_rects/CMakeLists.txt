cmake_minimum_required(VERSION 3.15)
project(random_rects)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Win32 GUI 程序: 不需要控制台窗口
add_executable(random_rects WIN32
    main.cpp
)

# 链接 Windows 库
target_link_libraries(random_rects
    PRIVATE
    gdi32
    user32
)
