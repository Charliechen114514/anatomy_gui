cmake_minimum_required(VERSION 3.15)

project(simple_notepad)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    user32
    gdi32
)

# Windows 特定设置
if(MSVC)
    # 设置字符集为 Unicode
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

    # 禁用一些警告
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
