---
title: 造骨架 · 从具象到抽象
---

# 造骨架

这是整部教程的主线：**亲手造一具 GUI 骨架**。

顺序是三步，不能颠倒：

1. **具象** —— 脱掉所有封装，用最裸的 Win32 把一个能用的程序写出来。你会亲眼看见 OS 的每一根线，也会亲手攒下一身别扭。
2. **抽象** —— 回头看那些别扭，一处一处把它们变成设计。这一层没有"新知识"，只有"为什么非这样不可"。
3. **落回具象** —— 用长出来的东西把同一个 demo 重写一遍。行为一模一样，代码短一大截。

> **状态**：**M1 完成** —— 具象一篇、抽象五篇、落回具象一篇、上屏专题一篇，共八篇；`gui_naked` 与 `gui` 两个可执行文件行为逐项一致。构建、格式、注释检查通过；tidy 待修（LLVM 23 新增检查）。

## 目录

| 篇 | 内容 |
|:--:|:---|
| 01 | [具象 · 脱掉封装，把窗口立起来](./01_naked_win32) |
| 02 | [抽象 1/5 · 给窗口找个家](./02_window_object) |
| 03 | [抽象 2/5 · 让控件成为一棵树](./03_widget_tree) |
| 04 | [抽象 3/5 · 消息不再自己处理](./04_event_routing) |
| 05 | [抽象 4/5 · 让代码算位置](./05_layout) |
| 06 | [抽象 5/5 · 把"记得重画"变成做不到](./06_property_repaint) |
| 07 | [落回具象 · 同一件事，两种写法](./07_two_ways) |
| 08 | [控件如何上屏](./08_widget_on_screen) —— 补上 M1 欠的那个问题 |

## 读到哪一篇，想补什么

主线不要求你先读 Win32 基础；卡住了再回来查。

| 读到 | 想补的底子 | 去哪几章 |
|:--:|:---|:---|
| 01 具象 | 程序本质、消息循环、控件、DPI、GDI | [0](/native_win32/0_ProgrammingGUI_Basic) · [1](/native_win32/1_ProgrammingGUI_NativeWindows) · [3](/native_win32/3_ProgrammingGUI_WhatAboutDPI) · [4](/native_win32/4_ProgrammingGUI_NativeWindows_Controls) · [18](/native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC) |
| 02 窗口对象 | 窗口创建与系统消息；全局状态怎么走到 OOP | [1](/native_win32/1_ProgrammingGUI_NativeWindows) · [2](/native_win32/2_ProgrammingGUI_NativeWindows_2) · 示例 `08_global_state` → `10_oop_wrapper` |
| 03 控件树 | 标准控件、通知消息 | [4](/native_win32/4_ProgrammingGUI_NativeWindows_Controls) · [5](/native_win32/5_ProgrammingGUI_NativeWindows_WM_NOTIFY) · [8](/native_win32/8_ProgrammingGUI_NativeWindows_MoreControls) |
| 04 事件路由 | 消息怎么分发、怎么跨线程 | [5](/native_win32/5_ProgrammingGUI_NativeWindows_WM_NOTIFY) · [59](/native_win32/59_ProgrammingGUI_NativeWindows_MsgMechanism) |
| 05 布局 | DPI 与坐标 | [3](/native_win32/3_ProgrammingGUI_WhatAboutDPI) |
| 06 属性与重画 | HDC、双缓冲、Alpha 混合 | [18](/native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC) · [23](/native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer) · [25](/native_win32/25_ProgrammingGUI_Graphics_GDI_AlphaBlend) |
| 07 落回具象 | 加控件、自绘、命中测试 | [4](/native_win32/4_ProgrammingGUI_NativeWindows_Controls) · [48](/native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw) · [50](/native_win32/50_ProgrammingGUI_Graphics_CustomCtrl_HitTest) |
| 08 上屏 | HDC、双缓冲、D2D、DWrite | [18](/native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC) · [23](/native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer) · [29](/native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture) · [32](/native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography) |

## 怎么观察每一层

- **具象**（裸版）：`src/gui/app/m1a_naked.cpp` 永久冻结，单独编译成 `gui_naked.exe` —— 可以和框架版并排跑
- **抽象五层**：每篇末尾的附录就是**写那一篇时的完整文件**（那一层的快照）；文中摘录是那一刻的代码
- **整体**：`git checkout m1` 拿到 M1 完成时的全貌
- **从 M2 起**：每一档打一个 tag，`git diff m2-1 m2-2` 就是那一档的完整改动

## 代码在哪

```
CMakeLists.txt                 顶层：工具链自检、编译选项
src/gui/CMakeLists.txt         目标定义
src/gui/app/m1a_naked.cpp      第 1 步的全部代码，一个文件
src/gui/app/demo.cpp           框架版：同一个 demo
src/gui/framework/             窗口对象、控件树、事件、布局、属性
src/gui/platform/              窗口、原生控件宿主
src/gui/app/gui.manifest        DPI 感知 + 视觉样式
```

构建（两条命令，不用预设）：

```bash
cmake -G Ninja -B build -S .
cmake --build build -j
```

产物落在 `build/bin/gui.exe`。
