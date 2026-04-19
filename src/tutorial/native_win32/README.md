# Win32 原生编程 · 代码示例

本目录包含 Win32 GUI 编程教程的配套代码示例。

## 示例结构

### 渐进式示例（01-10）

逐步构建一个完整的 Win32 程序，每个示例在前一个基础上新增一个概念：

| 示例 | 主题 | 对应教程 |
|:-----|:-----|:---------|
| 01_hello_world | Hello World 完整程序 | 第 1 章 |
| 02_register_window_class | 注册窗口类 | 第 1 章 |
| 03_create_window | 创建窗口 | 第 1 章 |
| 04_message_loop | 消息循环 | 第 1-2 章 |
| 05_window_procedure | 窗口过程 | 第 2 章 |
| 06_paint | 绘制（WM_PAINT） | 第 1 章 |
| 07_close_window | 关闭窗口 | 第 2 章 |
| 08_global_state | 全局状态管理 | 第 1 章 |
| 09_instance_data | 实例数据 | 第 1 章 |
| 10_oop_wrapper | OOP 封装（BaseWindow 模板） | 第 1 章 |

### 练习项目（exercises/）

| 示例 | 主题 | 涉及知识点 |
|:-----|:-----|:-----------|
| 01_click_counter | 点击计数器 | 消息处理、计数状态 |
| 02_random_rects | 随机方块画板 | GDI 绘制、定时器 |
| 03_mouse_tracker | 鼠标追踪器 | 鼠标消息、GDI |
| 04_simple_notepad | 简单记事本 | 控件、菜单、文件操作 |
| 05_draggable_ball | 可拖动小球 | 鼠标拖拽、双缓冲 |
| 06_double_buffer | 双缓冲消除闪烁 | 双缓冲技术 |

### 公共库

- `common/BaseWindow.hpp` — CRTP 模板类，封装 Win32 窗口过程，支持面向对象风格开发

## 构建方法

### 前提条件

- Visual Studio 2022（含「使用 C++ 的桌面开发」工作负载）
- CMake 3.20+

### 一键构建

```bash
# Windows CMD / PowerShell
build.bat
```

### 手动构建

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --config Release

# 运行某个示例
./build/01_hello_world/bin/01_hello_world.exe
```

## 编码规范

- C++20 标准
- Unicode 字符集（UNICODE / _UNICODE）
- 代码格式化使用 `.clang-format`（Microsoft 风格）
