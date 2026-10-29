cmake_minimum_required(VERSION 3.20)

# 01_click_counter - Win32 Click Counter 练习
#
# 功能:
# - 窗口中央显示当前点击次数
# - 左键点击增加计数并刷新显示
# - 右键点击重置计数
# - 使用 DrawText 居中显示大字体文本
#
# 技术要点:
# - 全局变量保存点击计数
# - 处理 WM_LBUTTONDOWN 消息
# - 处理 WM_RBUTTONDOWN 消息（重置）
# - 调用 InvalidateRect 触发重绘
# - 在 WM_PAINT 中使用 DrawText (DT_CENTER | DT_VCENTER | DT_SINGLELINE)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(click_counter
    main.cpp
)

target_compile_definitions(click_counter PRIVATE
    UNICODE
    _UNICODE
)

# 设置为 Windows GUI 应用（无控制台窗口）
set_target_properties(click_counter PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# 输出名称
set_target_properties(click_counter PROPERTIES
    OUTPUT_NAME "01_click_counter"
)

# MinGW: 使用 -municode 支持 wWinMain 入口点
if(NOT MSVC)
    target_link_options(click_counter PRIVATE -municode)
endif()
