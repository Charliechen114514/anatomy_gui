# Win32 消息循环示例程序

cmake_minimum_required(VERSION 3.15)

# 项目信息
project(MessageLoopExample
    VERSION 1.0.0
    DESCRIPTION "Win32 消息循环演示程序"
    LANGUAGES CXX
)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 特定设置
if(WIN32)
    # 设置为 Unicode 编码
    add_definitions(-DUNICODE -D_UNICODE)

    # 链接 Windows 库
    # user32: 窗口管理和消息处理
    # gdi32: 图形设备接口
    list(APPEND CMAKE_STANDARD_LIBRARIES
        user32
        gdi32
    )
endif()

# 源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    WIN32           # 创建 Windows GUI 应用程序（不显示控制台）
    ${SOURCES}
)

# 如果需要看到控制台输出，注释掉上面的 WIN32，使用下面这行:
# add_executable(${PROJECT_NAME} ${SOURCES})

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    # MSVC 编译选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4          # 警告级别 4
        /WX          # 将警告视为错误
        /utf-8       # 源文件使用 UTF-8 编码
    )
else()
    # GCC/Clang 编译选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
    )
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()

# 调试信息
message(STATUS "=========================================")
message(STATUS "项目: ${PROJECT_NAME}")
message(STATUS "描述: Win32 消息循环演示程序")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "配置类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "=========================================")
message(STATUS "")
message(STATUS "本程序演示以下内容:")
message(STATUS "  - GetMessage: 从消息队列获取消息")
message(STATUS "  - TranslateMessage: 虚拟键转字符消息")
message(STATUS "  - DispatchMessage: 分发消息到窗口过程")
message(STATUS "")
message(STATUS "构建步骤:")
message(STATUS "  1. mkdir build && cd build")
message(STATUS "  2. cmake ..")
message(STATUS "  3. cmake --build . --config Release")
message(STATUS "=========================================")
