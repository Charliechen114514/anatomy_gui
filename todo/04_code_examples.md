# Phase 4 — 代码示例基础设施

## 4.1 代码示例总览 README

**新文件**：`src/tutorial/native_win32/README.md`

```markdown
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
```

---

## 4.2 每个示例添加 README（16 个文件）

### 01_hello_world/README.md

```markdown
# 01_hello_world

Win32 Hello World — 完整的最小窗口程序。

对应章节：[第 1 章 消息机制与窗口创建](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- WinMain 入口函数
- RegisterClass 注册窗口类
- CreateWindow 创建窗口
- GetMessage / TranslateMessage / DispatchMessage 消息循环
- DefWindowProc 默认窗口过程
```

### 02_register_window_class/README.md

```markdown
# 02_register_window_class

窗口类注册详解 —WNDCLASSEX 各字段的作用与配置。

对应章节：[第 1 章 消息机制与窗口创建](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- WNDCLASSEX 结构体字段详解
- 窗口类名称与实例句柄
- 光标、图标、背景画刷的默认设置
- RegisterClassEx 返回值与错误处理
```

### 03_create_window/README.md

```markdown
# 03_create_window

CreateWindow 参数详解 —窗口风格的组合与意义。

对应章节：[第 1 章 消息机制与窗口创建](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- CreateWindowEx 各参数含义
- WS_OVERLAPPEDWINDOW 等常用风格
- ShowWindow 与 UpdateWindow
- 窗口创建消息（WM_CREATE / WM_NCCREATE）
```

### 04_message_loop/README.md

```markdown
# 04_message_loop

消息循环机制 —Windows 消息泵的工作原理。

对应章节：[第 1-2 章](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- GetMessage / PeekMessage 区别
- TranslateMessage 键盘消息翻译
- DispatchMessage 消息分发
- WM_QUIT 与消息循环退出
```

### 05_window_procedure/README.md

```markdown
# 05_window_procedure

窗口过程函数 —处理窗口消息的核心回调。

对应章节：[第 2 章 常用系统消息](../../../tutorial/native_win32/2_ProgrammingGUI_NativeWindows_2.md)

## 学习要点

- WndProc 签名与参数含义
- switch-case 消息处理模式
- WM_PAINT / WM_DESTROY 基本处理
- DefWindowProc 默认处理的重要性
```

### 06_paint/README.md

```markdown
# 06_paint

WM_PAINT 与基本绘制 —在窗口客户区绘图。

对应章节：[第 1 章](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- BeginPaint / EndPaint 配对使用
- PAINTSTRUCT 结构体
- TextOut 基本文字输出
- 无效区域与 InvalidateRect
```

### 07_close_window/README.md

```markdown
# 07_close_window

窗口关闭流程 —从 WM_CLOSE 到程序退出的完整链路。

对应章节：[第 2 章 常用系统消息](../../../tutorial/native_win32/2_ProgrammingGUI_NativeWindows_2.md)

## 学习要点

- WM_CLOSE / WM_DESTROY / WM_NCDESTROY 的顺序
- PostQuitMessage 退出消息循环
- 关闭确认（用户点击 X 后的拦截）
- DestroyWindow 与资源释放
```

### 08_global_state/README.md

```markdown
# 08_global_state

全局状态管理 —使用全局变量在窗口过程中共享数据。

对应章节：[第 1 章](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- 全局变量在 Win32 程序中的角色
- 多实例共享问题
- 线程安全考虑
- 何时使用全局状态 vs 实例数据
```

### 09_instance_data/README.md

```markdown
# 09_instance_data

实例数据管理 —通过 GWLP_USERDATA 或窗口属性存储实例数据。

对应章节：[第 1 章](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- SetWindowLongPtr / GetWindowLongPtr
- GWLP_USERDATA 存储this指针
- WM_NCCREATE 中设置实例数据
- 替代全局变量的标准模式
```

### 10_oop_wrapper/README.md

```markdown
# 10_oop_wrapper

OOP 封装 —使用 CRTP 模板封装 Win32 窗口类。

对应章节：[第 1 章](../../../tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)

## 学习要点

- BaseWindow<T> CRTP 模板设计
- 静态 WindowProc 转发到成员函数
- HandleMessage 虚函数模式
- 面向对象的 Win32 编程范式
```

### exercises/01_click_counter/README.md

```markdown
# 练习 1：点击计数器

点击窗口客户区，显示累计点击次数。练习消息处理与状态管理。

涉及知识点：WM_LBUTTONDOWN、字符串格式化、InvalidateRect 重绘
```

### exercises/02_random_rects/README.md

```markdown
# 练习 2：随机方块画板

定时在窗口中绘制随机颜色和位置的矩形。练习定时器与 GDI 绘制。

涉及知识点：SetTimer / WM_TIMER、CreateSolidBrush、FillRect
```

### exercises/03_mouse_tracker/README.md

```markdown
# 练习 3：鼠标追踪器

实时显示鼠标位置和移动轨迹。练习鼠标消息处理。

涉及知识点：WM_MOUSEMOVE、WM_LBUTTONDOWN / WM_LBUTTONUP、MoveToEx / LineTo
```

### exercises/04_simple_notepad/README.md

```markdown
# 练习 4：简单记事本

实现一个带菜单栏的简易文本编辑器。练习控件与文件操作。

涉及知识点：Edit/RichEdit 控件、菜单资源、GetOpenFileName / GetSaveFileName
```

### exercises/05_draggable_ball/README.md

```markdown
# 练习 5：可拖动小球

实现一个可以用鼠标拖动的圆形。练习鼠标拖拽与 GDI。

涉及知识点：鼠标拖拽（SetCapture / ReleaseCapture）、Ellipse 绘制、碰撞检测
```

### exercises/06_double_buffer/README.md

```markdown
# 练习 6：双缓冲消除闪烁

对比有/无双缓冲的绘制效果。练习双缓冲技术。

涉及知识点：CreateCompatibleDC、CreateCompatibleBitmap、BitBlt、WM_ERASEBKGND
```

---

## 4.3 添加 .clang-format

**新文件**：`src/tutorial/native_win32/.clang-format`

```yaml
---
BasedOnStyle: Microsoft
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Inline
BreakBeforeBraces: Allman
PointerAlignment: Left
SortIncludes: false
AllowShortIfStatementsOnASingleLine: false
IndentCaseLabels: false
NamespaceIndentation: None
AccessModifierOffset: -4
```

**说明**：
- `Microsoft` 基础风格，适合 Windows API 代码
- `SortIncludes: false` 至关重要，Windows 头文件有严格顺序（windows.h 必须先于 commctrl.h 等）
- `Allman` 大括号换行风格，与 Win32 API 文档惯例一致

---

## 4.4 一键构建脚本

**新文件**：`src/tutorial/native_win32/build.bat`

```bat
@echo off
setlocal

echo ============================================
echo  Building Win32 Tutorial Examples
echo ============================================
echo.

echo [1/2] Configuring CMake...
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    exit /b %errorlevel%
)

echo.
echo [2/2] Building all examples (Release)...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    exit /b %errorlevel%
)

echo.
echo ============================================
echo  All examples built successfully!
echo  Executables are in build\bin\
echo ============================================
```

---

## 4.5 图形渲染系列代码示例（长期计划）

**现状**：第 18-52 章均无配套代码示例。

**优先级排序**：
1. GDI 系列（18-25）— 可复用现有 CMake 框架，链接 gdi32.lib
2. 自定义控件（48-50）— 纯 Win32，无额外依赖
3. Direct2D 系列（29-33）— 需链接 d2d1.lib, dwrite.lib
4. D3D11 系列（37-42）— 需链接 d3d11.lib, dxgi.lib, d3dcompiler.lib
5. GDI+ 系列（26-28）— 需链接 gdiplus.lib
6. HLSL 系列（34-36）— 需 d3dcompiler.lib + HLSL 文件
7. D3D12 系列（43-47）— 需链接 d3d12.lib, dxgi.lib, d3dcompiler.lib
8. OpenGL 系列（51-52）— 需链接 opengl32.lib，需第三方 GLAD/GLFW

**注意**：每个系列需要不同的 CMake 配置（链接库、编译定义）。建议创建 `src/tutorial/graphics/` 目录存放图形渲染相关示例。
