# GUI 通用编程教程

> 一套面向 C++ 开发者的图形界面编程完整路线图，从 Win32 原生 API 到现代跨平台框架，循序渐进，深度剖析。

## 为什么要做这个

说实话，现在想系统地学习 GUI 编程并不容易。市面上要么是零散的博客文章，要么是官方文档那种冷冰冰的参考手册，缺乏一套能把整个知识体系串起来的教程。很多人想学客户端开发，但不知道从哪入手——Electron 太重、Qt 文档太厚、Win32 看起来像上古遗迹。

这个项目的目的就是填补这个空白。我们从 Windows 最底层的 Win32 API 开始，一路讲到 Qt、WinUI、WebView2 等现代技术。你可能会问：为什么不直接学框架？因为只有理解了底层的消息机制、窗口模型、绘制原理，切换任何框架时都会顺畅很多。

---

## 学习路线

```
第〇篇  前言与准备          世界观建立
第一篇  Win32 原生编程      地基（进行中 ██████░ 65%）
第二篇  GUI 核心概念        跨框架通识
第三篇  图形渲染            GDI → Direct2D → GPU
第四篇  跨平台框架          Qt / wxWidgets / GTK
第五篇  现代 Windows 技术   WinUI 3 / WebView2
第六篇  Web 混合方案        Electron / CEF
第七篇  工程化专题          多线程、DPI、打包、i18n
第八篇  综合实战项目        贯穿全系列的收尾
```

---

## 技术覆盖

[![Win32](https://img.shields.io/badge/Win32-API-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![Qt](https://img.shields.io/badge/Qt-Framework-41CD52?style=for-the-badge&logo=qt&logoColor=white)]()
[![WinUI](https://img.shields.io/badge/WinUI-3-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![GTK](https://img.shields.io/badge/GTK-4.0-4DB33D?style=for-the-badge&logo=gtk&logoColor=white)]()
[![WebView2](https://img.shields.io/badge/WebView2-Chromium-00CE7C?style=for-the-badge&logo=chromium&logoColor=white)]()

---

## 第一篇 Win32 原生编程

### 基础篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 0 | Windows 程序的本质 | 已完成 | [0_ProgrammingGUI_Basic.md](native_win32/0_ProgrammingGUI_Basic.md) |
| 1 | 消息机制与窗口创建 | 已完成 | [1_ProgrammingGUI_NativeWindows.md](native_win32/1_ProgrammingGUI_NativeWindows.md) |
| 2 | 常用系统消息 | 已完成 | [2_ProgrammingGUI_NativeWindows_2.md](native_win32/2_ProgrammingGUI_NativeWindows_2.md) |
| 3 | DPI 适配专题 | 已完成 | [3_ProgrammingGUI_WhatAboutDPI.md](native_win32/3_ProgrammingGUI_WhatAboutDPI.md) |

### 控件篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 4 | 标准控件（按钮、编辑框等） | 已完成 | [4_ProgrammingGUI_NativeWindows_Controls.md](native_win32/4_ProgrammingGUI_NativeWindows_Controls.md) |
| 5 | WM_NOTIFY 通知消息 | 已完成 | [5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md](native_win32/5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md) |
| 6 | ListView 列表视图控件 | 已完成 | [6_ProgrammingGUI_NativeWindows_ListView.md](native_win32/6_ProgrammingGUI_NativeWindows_ListView.md) |
| 7 | TreeView 树形视图控件 | 已完成 | [7_ProgrammingGUI_NativeWindows_TreeView.md](native_win32/7_ProgrammingGUI_NativeWindows_TreeView.md) |
| 8 | 更多控件（进度条、滚动条等） | 已完成 | [8_ProgrammingGUI_NativeWindows_MoreControls.md](native_win32/8_ProgrammingGUI_NativeWindows_MoreControls.md) |

### 对话框篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 9 | 模态对话框 | 已完成 | [9_ProgrammingGUI_NativeWindows_ModalDialog.md](native_win32/9_ProgrammingGUI_NativeWindows_ModalDialog.md) |
| 10 | 非模态对话框 | 已完成 | [10_ProgrammingGUI_NativeWindows_ModelessDialog.md](native_win32/10_ProgrammingGUI_NativeWindows_ModelessDialog.md) |
| 11 | 对话框过程深入 | 已完成 | [11_ProgrammingGUI_NativeWindows_DialogProc.md](native_win32/11_ProgrammingGUI_NativeWindows_DialogProc.md) |

### 资源篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 12 | 资源文件完全指南 | 已完成 | [12_ProgrammingGUI_NativeWindows_ResourceFiles.md](native_win32/12_ProgrammingGUI_NativeWindows_ResourceFiles.md) |
| 13 | 菜单资源 | 已完成 | [13_ProgrammingGUI_NativeWindows_MenuResource.md](native_win32/13_ProgrammingGUI_NativeWindows_MenuResource.md) |
| 14 | 图标、光标与位图 | 已完成 | [14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md](native_win32/14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md) |
| 15 | 字符串表与国际化 | 已完成 | [15_ProgrammingGUI_NativeWindows_StringTable.md](native_win32/15_ProgrammingGUI_NativeWindows_StringTable.md) |
| 16 | 对话框模板 | 已完成 | [16_ProgrammingGUI_NativeWindows_DialogTemplate.md](native_win32/16_ProgrammingGUI_NativeWindows_DialogTemplate.md) |
| 17 | VS 资源编辑器 | 已完成 | [17_ProgrammingGUI_NativeWindows_VSResourceEditor.md](native_win32/17_ProgrammingGUI_NativeWindows_VSResourceEditor.md) |

### 配套示例代码

每个章节都配有完整的可运行示例，位于 [src/tutorial/native_win32/](../src/tutorial/native_win32/) 目录：

- **基础示例**：01_hello_world ~ 10_oop_wrapper（10个渐进式示例）
- **练习项目**：6个实战练习（点击计数器、随机方块、鼠标追踪器、简单记事本、拖动小球、双缓冲）

---

## 开发环境

### 必需组件

- **IDE**: Visual Studio 2022 Community（免费）
- **SDK**: Windows SDK（随 VS 自动安装）
- **编译器**: MSVC（支持 32/64 位）
- **构建系统**: CMake 3.20+

### 安装步骤

1. 安装 **Visual Studio 2022 Community**
2. 在安装器中勾选「使用 C++ 的桌面开发」工作负载
3. 确保包含 Windows 10/11 SDK 和 CMake 工具

### 构建项目

```bash
# 克隆仓库后
cd gui/src/tutorial/native_win32

# 配置（生成 compile_commands.json）
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 构建
cmake --build build --config Release

# 运行示例
./build/01_hello_world/bin/01_hello_world.exe
```

每个示例独立可构建，支持 VS Code + CMake Tools 或 Visual Studio 直接打开。

---

## 参考资源

[![Microsoft Learn](https://img.shields.io/badge/Microsoft-Learn-0078D4?style=flat-square&logo=microsoft&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/)
[![Windows API](https://img.shields.io/badge/Windows-API-0078D4?style=flat-square&logo=windows&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/apiindex/)

> 一直在路上，持续更新中...
