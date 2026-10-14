# M1a · 具象 —— 裸 Win32，一个文件，故意不做任何封装。
#
# 纪律（见 todo/architecture.md 第一节）：这一节的代码里不许出现承载状态的
# class / struct。允许自由函数 —— 但状态就该是全局的、消息就该是一个大 switch。
#
# The manifest carries the DPI awareness and the Common Controls v6
# dependency; see app/gui.manifest.
add_executable(gui_naked WIN32
    app/m1a_naked.cpp
    app/gui.manifest)

# M1b · 抽象 —— 从 gui_naked 一层一层长出封装（第 2 步，五小步）。
#
# 层的规矩：framework/ 里不出现 windows.h；凡是持有平台句柄的类都住在
# platform/。所以 NativeWidget 在 platform/，不在 framework/。
add_executable(gui WIN32
    app/demo.cpp
    framework/widget.cpp
    framework/event_router.cpp
    framework/stack_layout.cpp
    platform/window.cpp
    platform/native_widget.cpp
    app/gui.manifest)
target_include_directories(gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 产物统一落 build/bin，双击就能跑（多配置生成器会再套一层 build/bin/Debug）。
set_target_properties(gui gui_naked PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
