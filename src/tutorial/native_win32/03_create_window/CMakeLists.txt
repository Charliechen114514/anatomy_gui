# 03_create_window - 创建窗口演示
# 本示例展示 CreateWindowEx 函数的各种参数和窗口样式

cmake_minimum_required(VERSION 3.20)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 定义可执行文件
add_executable(03_create_window
    main.cpp
)

# 设置输出目录
set_target_properties(03_create_window PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 链接 Windows GUI 库
# user32.dll: 包含 CreateWindowEx, RegisterClassEx, GetMessage 等函数
# gdi32.dll: 包含绘图相关函数 (SetTextColor, DrawText 等)
target_link_libraries(03_create_window
    PRIVATE
    user32
    gdi32
)

# 设置子系统为 Windows (不显示控制台窗口)
if(MSVC)
    set_target_properties(03_create_window PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
else()
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(03_create_window PRIVATE -municode)
endif()

# 设置 C++20 标准
target_compile_features(03_create_window PRIVATE cxx_std_20)

# 设置 Unicode 定义
target_compile_definitions(03_create_window
    PRIVATE
    UNICODE
    _UNICODE
)

# 设置警告级别
if(MSVC)
    target_compile_options(03_create_window PRIVATE /W4)
else()
    target_compile_options(03_create_window PRIVATE -Wall -Wextra)
endif()
