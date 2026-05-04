cmake_minimum_required(VERSION 3.20)

project(NativeWin32Tutorials VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json 用于 IDE/语言服务器
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows Unicode 设置
add_definitions(-DUNICODE -D_UNICODE)

# MSVC 编译选项
if(MSVC)
    add_compile_options(/utf-8)
else()
    # MinGW: 资源编译器使用 UTF-8 代码页
    set(CMAKE_RC_FLAGS "--codepage=65001")
endif()

# 不使用控制台（所有示例都是 GUI 应用）
set(CMAKE_WIN32_EXECUTABLE ON)

# 通用设置：禁用某些默认的 IDE 文件夹
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 辅助类库（仅头文件）
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# =============================================================================
# 主体章节示例 01-10
# =============================================================================

# 01_hello_world - 完整 Hello World 程序
add_subdirectory(01_hello_world)

# 02_register_window_class - 注册窗口类演示
add_subdirectory(02_register_window_class)

# 03_create_window - 创建窗口演示
add_subdirectory(03_create_window)

# 04_message_loop - 消息循环演示
add_subdirectory(04_message_loop)

# 05_window_procedure - 窗口过程演示
add_subdirectory(05_window_procedure)

# 06_paint - 绘制窗口演示
add_subdirectory(06_paint)

# 07_close_window - 关闭窗口演示
add_subdirectory(07_close_window)

# 08_global_state - 全局变量状态管理
add_subdirectory(08_global_state)

# 09_instance_data - 实例数据状态管理
add_subdirectory(09_instance_data)

# 10_oop_wrapper - 面向对象封装
add_subdirectory(10_oop_wrapper)

# =============================================================================
# 练习项目 exercises/
# =============================================================================

# 01_click_counter - 练习1: 点击计数器
add_subdirectory(exercises/01_click_counter)

# 02_random_rects - 练习2: 随机方块画板
add_subdirectory(exercises/02_random_rects)

# 03_mouse_tracker - 练习3: 鼠标追踪器
add_subdirectory(exercises/03_mouse_tracker)

# 04_simple_notepad - 练习4: 简单记事本
add_subdirectory(exercises/04_simple_notepad)

# 05_draggable_ball - 练习5: 可拖动小球
add_subdirectory(exercises/05_draggable_ball)

# 06_double_buffer - 练习6: 双缓冲消除闪烁
add_subdirectory(exercises/06_double_buffer)

# =============================================================================
# 扩展示例 11-25
# =============================================================================

# 11_dpi_aware - DPI 感知（Per-Monitor V2）
add_subdirectory(11_dpi_aware)

# 12_basic_controls - 基础控件（Button/Edit/ListBox/ComboBox）
add_subdirectory(12_basic_controls)

# 13_wm_notify - WM_NOTIFY 通知机制
add_subdirectory(13_wm_notify)

# 14_listview - ListView 报表视图
add_subdirectory(14_listview)

# 15_treeview - TreeView 树形控件
add_subdirectory(15_treeview)

# 16_more_controls - 更多控件（ProgressBar/TrackBar/UpDown/Tab）
add_subdirectory(16_more_controls)

# 17_modal_dialog - 模态对话框
add_subdirectory(17_modal_dialog)

# 18_modeless_dialog - 无模态对话框
add_subdirectory(18_modeless_dialog)

# 19_dialog_proc - DialogProc 高级模式
add_subdirectory(19_dialog_proc)

# 20_resource_files - .rc 资源文件基础
add_subdirectory(20_resource_files)

# 21_menu_resource - 菜单资源
add_subdirectory(21_menu_resource)

# 22_icon_cursor_bitmap - 图标/光标/位图资源
add_subdirectory(22_icon_cursor_bitmap)

# 23_string_table - 字符串表资源
add_subdirectory(23_string_table)

# 24_dialog_template - 对话框模板资源
add_subdirectory(24_dialog_template)

# 25_toolbar_statusbar - 工具栏 + 状态栏
add_subdirectory(25_toolbar_statusbar)
