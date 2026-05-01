# Phase 0 — 现有内容清理

> 前置步骤，必须在所有新内容创建之前完成。

## 0A. Qt 内容移除

### 必须删除

- [ ] 删除 `tutorial/native_win32/52_ProgrammingGUI_Graphics_OpenGL_QtGLWidget.md`（整篇 Qt QOpenGLWidget 教程，467 行）

### 必须重写

- [ ] 重写 `tutorial/native_win32/3_ProgrammingGUI_WhatAboutDPI.md`
  - 标题从"Windows和Qt双平台实战指南"改为纯 Windows
  - 移除"第三步 Qt是怎么处理这个问题的"（L72-96）
  - 移除"第五步 Qt实战：从入门到正确"（L267-358）
  - 移除"收尾"中的 Qt 推荐段落（L443-449）
  - 移除"踩坑预警"中混合的 Qt 内容
  - 移除"环境说明"中的 Qt 版本行
  - 保留所有 Windows DPI 内容（概念、模式、Win32 实战）
  - 重写总结段落，不再推荐 Qt

### 必须修改（小范围）

- [ ] `tutorial/native_win32/51_ProgrammingGUI_Graphics_OpenGL_Win32.md` L371
  - 移除"接下来，我们要把 OpenGL 的 Win32 原生封装升级到跨平台框架——Qt 的 QOpenGLWidget..."
  - 替换为合适的下节预告或直接结束

### 轻微修改（Qt 提及清理）

- [ ] `tutorial/native_win32/0_ProgrammingGUI_Basic.md` — 前言中"Electron 一把梭"等表述，更新为不提 Qt 的版本
- [ ] `tutorial/native_win32/4_ProgrammingGUI_NativeWindows_Controls.md` — "现代框架像 Qt、WPF"，删除 Qt
- [ ] `tutorial/native_win32/5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md` — "Qt、WPF、GTK"，删除 Qt

### 全局文件更新

- [ ] `Roadmap.md` — 移除 Qt（4.1-4.8 共 8 节）、wxWidgets（4.9-4.10 共 2 节）、Electron/Tauri（6.3-6.4）、Accessibility（7.6），重新编号
- [ ] `README.md` — 移除 Qt 技术徽章，更新学习路线描述
- [ ] `tutorial/index.md` — 移除 ch52 行，更新 OpenGL 篇说明

---

## 0B. 代码示例 C++ 标准统一

> 根 CMakeLists.txt 声明 C++20，但子项目各自覆盖了 C++11/C++17。

### 需要修改的 CMakeLists.txt（移除 `CMAKE_CXX_STANDARD` 设置，继承根配置）

- [ ] `src/tutorial/native_win32/02_register_window_class/CMakeLists.txt`（当前 C++11）
- [ ] `src/tutorial/native_win32/05_window_procedure/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/07_close_window/CMakeLists.txt`（当前 C++11）
- [ ] `src/tutorial/native_win32/08_global_state/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/09_instance_data/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/exercises/02_random_rects/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/exercises/03_mouse_tracker/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/exercises/04_simple_notepad/CMakeLists.txt`（当前 C++17）
- [ ] `src/tutorial/native_win32/exercises/06_double_buffer/CMakeLists.txt`（当前 C++17）

### 无需修改

- `01_hello_world`（C++20）
- `03_create_window`（C++20）
- `04_message_loop`（C++20）
- `06_paint`（继承根）
- `10_oop_wrapper`（继承根）
- `exercises/01_click_counter`（继承根）
- `exercises/05_draggable_ball`（继承根）

---

## 0C. 其他已知问题

- [ ] `tutorial/tags.md` — 当前仅有占位符 `[TAGS]`，需填充实际标签分类体系
- [ ] `tutorial/native_win32/17_ProgrammingGUI_NativeWindows_VSResourceEditor.md` — 检查 TODO 注释是否为预期占位

---

## 验证

Phase 0 完成后执行：

```bash
# 1. 文档构建
pip install -r requirements.txt && mkdocs build --clean --strict

# 2. 代码编译（Windows）
cd src/tutorial/native_win32 && cmake -B build && cmake --build build --config Release
```
