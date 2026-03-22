cmake_minimum_required(VERSION 3.20)

# 05_draggable_ball - 可拖动小球练习
#
# 练习内容:
# - 鼠标消息处理 (WM_LBUTTONDOWN, WM_MOUSEMOVE, WM_LBUTTONUP)
# - 点到圆的距离检测
# - 光标切换 (SetCursor, LoadCursor)
# - 触发重绘 (InvalidateRect)
# - GDI 绘图 (Ellipse)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(draggable_ball
    main.cpp
)

target_compile_definitions(draggable_ball PRIVATE
    UNICODE
    _UNICODE
)

# 设置为 Windows GUI 应用（无控制台窗口）
set_target_properties(draggable_ball PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# 输出名称
set_target_properties(draggable_ball PROPERTIES
    OUTPUT_NAME "05_draggable_ball"
)

# MinGW: 使用 -municode 支持 wWinMain 入口点
if(NOT MSVC)
    target_link_options(draggable_ball PRIVATE -municode)
endif()
