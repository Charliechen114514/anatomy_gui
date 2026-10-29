# 01_hello_world - Win32 Hello World 示例

cmake_minimum_required(VERSION 3.15)
project(01_hello_world)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# 链接 Windows 库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    user32
    gdi32
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4          # 警告级别
        /WX          # 将警告视为错误
        /utf-8       # 使用 UTF-8 编码
    )
    # 设置入口点为 Unicode 版本的 WinMain
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
    )
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()
