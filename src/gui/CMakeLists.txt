# M1a · 具象 —— 裸 Win32，一个文件，故意不做任何封装。
#
# 纪律（见 todo/architecture.md 第一节）：这一节的代码里不许出现承载状态的
# class / struct。允许自由函数 —— 但状态就该是全局的、消息就该是一个大 switch。

add_executable(gui WIN32 app/m1a_naked.cpp)

# 产物统一落 build/bin，双击就能跑（多配置生成器会再套一层 build/bin/Debug）。
set_target_properties(gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
