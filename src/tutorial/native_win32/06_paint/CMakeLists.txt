cmake_minimum_required(VERSION 3.20)

# 06_paint - Win32 GDI 绘图示例
#
# 演示内容:
# - WM_PAINT 消息处理 (BeginPaint/EndPaint)
# - GDI 绘图函数 (TextOut, Rectangle, Ellipse, LineTo)
# - GDI 对象管理 (CreateSolidBrush, DeleteObject)
# - 更新区域的概念
# - 定时器触发重绘

cmake_minimum_required(VERSION 3.20)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(paint
    main.cpp
)

target_compile_definitions(paint PRIVATE
    UNICODE
    _UNICODE
)

# 设置为 Windows GUI 应用（无控制台窗口）
set_target_properties(paint PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# MinGW: 使用 -municode 支持 wWinMain 入口点
if(NOT MSVC)
    target_link_options(paint PRIVATE -municode)
endif()

# 输出名称
set_target_properties(paint PROPERTIES
    OUTPUT_NAME "06_paint"
)
