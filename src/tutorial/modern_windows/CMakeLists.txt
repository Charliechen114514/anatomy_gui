# ModernWindows —— 现代 C++ / 协程 / Win32 GUI 框架剖析
#
# 构建方式（VS Code + CMake Tools，或命令行）：
#   cmake --preset default            # 配置（Ninja，导出 compile_commands.json）
#   cmake --build --preset default    # 构建

cmake_minimum_required(VERSION 3.26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(ModernWindows LANGUAGES CXX)

# --- 全系列工具链基线：拉到最新标准 + UTF-8 源码/执行字符集 + UNICODE ---
# 放在顶层，所有子目录的 target 自动继承，避免某个 stage 漏掉 /utf-8 导致中文乱码。
add_compile_options(/std:c++latest /utf-8)
add_compile_definitions(UNICODE _UNICODE)

# --- 工具链自检 ---
add_subdirectory(00_bootstrap)

# --- 每个阶段自包含：写好一个 stageNN 目录，就在这里加一行 add_subdirectory(stageNN_xxx)
add_subdirectory(stage01_window_surface)
# add_subdirectory(stage02_message_coroutine)
# add_subdirectory(stage03_ownership)
