# Win32 关闭窗口示例程序配置

cmake_minimum_required(VERSION 3.15)

# 项目名称
project(close_window)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 可执行文件
add_executable(${PROJECT_NAME}
    main.cpp
)

# 链接 Win32 库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    user32
    gdi32
)

# 设置子系统为 Windows (不显示控制台窗口)
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
else()
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()
