# Win32 窗口类注册示例程序
# 演示如何使用 WNDCLASS 结构和 RegisterClass 函数注册窗口类

cmake_minimum_required(VERSION 3.15)

# 项目名称和语言
project(02_register_window_class
    VERSION 1.0.0
    DESCRIPTION "Win32 Window Class Registration Example"
    LANGUAGES CXX
)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# 链接 Windows 库
# user32.dll 包含 RegisterClass, CreateWindow, GetMessage 等核心窗口函数
# gdi32.dll 包含绘图相关函数（如 DrawText）
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    user32
    gdi32
)

# 编译选项
if(MSVC)
    # 使用 Unicode 字符集
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

    # 警告级别
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)

    # 禁用一些不常用的警告
    target_compile_options(${PROJECT_NAME} PRIVATE
        /wd4996  # 禁用 "不安全函数" 警告（如 wsprintf）
    )
else()
    # MinGW/GCC 配置
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装规则（可选）
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
