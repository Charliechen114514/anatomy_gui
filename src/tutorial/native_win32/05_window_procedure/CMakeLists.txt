cmake_minimum_required(VERSION 3.15)
project(WindowProcedureDemo)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Windows 特定设置
if(WIN32)
    # 链接 Windows 库
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        gdi32
        user32
    )
endif()

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
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
    )
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()
