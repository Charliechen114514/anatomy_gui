# GUI 通用编程指南

[![Platform](https://img.shields.io/badge/platform-Windows-0078D4?style=flat-square&logo=windows&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/)
[![Language](https://img.shields.io/badge/language-C++-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![IDE](https://img.shields.io/badge/IDE-Visual%20Studio-5C2D91?style=flat-square&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Win32](https://img.shields.io/badge/Win32-75%25-orange?style=flat-square)](tutorial/native_win32/)
[![Graphics](https://img.shields.io/badge/Graphics-95%25-brightgreen?style=flat-square)](tutorial/native_win32/)
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> 一套面向 C++ 开发者的图形界面编程完整路线图，从 Win32 原生 API 到现代跨平台框架，循序渐进，深度剖析。

---

## 目录

- [在线阅读](#在线阅读)
- [为什么要做这个](#为什么要做这个)
- [技术覆盖](#技术覆盖)
- [学习路线](#学习路线)
- [内容总览](#内容总览)
- [配套示例代码](#配套示例代码)
- [开发环境](#开发环境)
- [贡献](#贡献)
- [许可证](#许可证)

---

## 在线阅读

在线阅读地址：[anatomy_gui 教程站](https://charliechen114514.github.io/anatomy_gui/)

---

## 为什么要做这个

说实话，现在想系统地学习 GUI 编程并不容易。市面上要么是零散的博客文章，要么是官方文档那种冷冰冰的参考手册，缺乏一套能把整个知识体系串起来的教程。很多人想学客户端开发，但不知道从哪入手——Electron 太重、Qt 文档太厚、Win32 看起来像上古遗迹。

这个项目的目的就是填补这个空白。我们从 Windows 最底层的 Win32 API 开始，一路讲到 Qt、WinUI、WebView2 等现代技术。你可能会问：为什么不直接学框架？因为只有理解了底层的消息机制、窗口模型、绘制原理，切换任何框架时都会顺畅很多。

---

## 技术覆盖

[![Win32](https://img.shields.io/badge/Win32-API-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![GDI](https://img.shields.io/badge/GDI-Graphics-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![Direct2D](https://img.shields.io/badge/Direct2D-Hardware_Accel-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![D3D11](https://img.shields.io/badge/Direct3D_11-3D_Rendering-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![D3D12](https://img.shields.io/badge/Direct3D_12-Low_Level-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![OpenGL](https://img.shields.io/badge/OpenGL-Cross_Platform-5586A4?style=for-the-badge&logo=opengl&logoColor=white)]()
[![Qt](https://img.shields.io/badge/Qt-Framework-41CD52?style=for-the-badge&logo=qt&logoColor=white)]()
[![WinUI](https://img.shields.io/badge/WinUI-3-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()

---

## 学习路线

```
第〇篇  前言与准备           世界观建立
第一篇  Win32 原生编程       地基（进行中 ███████░ 75%）
第二篇  GUI 核心概念         跨框架通识
第三篇  图形渲染             GDI → Direct2D → GPU（进行中 █████████░ 95%）
第四篇  跨平台框架           Qt / wxWidgets / GTK
第五篇  现代 Windows 技术    WinUI 3 / WebView2
第六篇  Web 混合方案         Electron / CEF / Tauri
第七篇  工程化专题           多线程、DPI、打包、i18n
第八篇  综合实战项目         贯穿全系列的收尾
```

---

## 内容总览

### 第一篇 · Win32 原生编程（19 章，进行中）

| 系列 | 章节范围 | 章节数 | 状态 |
|:-----|:---------|:-------|:-----|
| 基础篇 | 0-3 | 4 | 已完成 |
| 控件篇 | 4-8 | 5 | 已完成 |
| 对话框篇 | 9-11 | 3 | 已完成 |
| 资源篇 | 12-17 | 6 | 已完成 |
| 工具栏与状态栏 | 17.5 | 1 | 已完成 |
| 进阶篇（子类化/Hook/托盘/拖放/定时器） | — | — | 规划中 |

### 第三篇 · 图形渲染（35 章，进行中）

| 系列 | 章节范围 | 章节数 | 状态 |
|:-----|:---------|:-------|:-----|
| GDI 图形 | 18-25 | 8 | 已完成 |
| GDI+ | 26-28 | 3 | 已完成 |
| Direct2D / DirectWrite | 29-33 | 5 | 已完成 |
| HLSL 着色器 | 34-36 | 3 | 已完成 |
| Direct3D 11 | 37-42 | 6 | 已完成 |
| Direct3D 12 | 43-47 | 5 | 已完成 |
| 自定义控件 | 48-50 | 3 | 已完成 |
| OpenGL | 51-52 | 2 | 已完成 |

更多细节请查看 [Roadmap.md](Roadmap.md) 完整规划。

---

## 配套示例代码

每个章节都配有完整的可运行示例，位于 [src/tutorial/native_win32/](src/tutorial/native_win32/) 目录：

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

## 贡献

欢迎贡献！请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解如何参与。

---

## 参考资源

[![Microsoft Learn](https://img.shields.io/badge/Microsoft-Learn-0078D4?style=flat-square&logo=microsoft&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/)
[![Windows API](https://img.shields.io/badge/Windows-API-0078D4?style=flat-square&logo=windows&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/apiindex/)

---

## 许可证

Copyright © 2024-2025 [Charliechen114514](https://github.com/Charliechen114514)

本项目采用 [MIT License](LICENSE) 开源协议。

---

> 一直在路上，持续更新中...
