# GUI 通用编程指南

[![Platform](https://img.shields.io/badge/platform-Windows-0078D4?style=flat-square&logo=windows&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/)
[![Language](https://img.shields.io/badge/language-C++-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![IDE](https://img.shields.io/badge/IDE-Visual%20Studio-5C2D91?style=flat-square&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Progress](https://img.shields.io/badge/Win32-65%25-orange?style=flat-square)](Roadmap.md)
[![Writing](https://img.shields.io/badge/Writing-In%20Progress-brightgreen?style=flat-square)](tutorial/native_win32/)
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> 一套面向 C++ 开发者的图形界面编程完整路线图，从 Win32 原生 API 到现代跨平台框架，循序渐进，深度剖析。

---

## 为什么要做这个

说实话，现在想系统地学习 GUI 编程并不容易。市面上要么是零散的博客文章，要么是官方文档那种冷冰冰的参考手册，缺乏一套能把整个知识体系串起来的教程。很多人想学客户端开发，但不知道从哪入手——Electron 太重、Qt 文档太厚、Win32 看起来像上古遗迹。

这个项目的目的就是填补这个空白。我们从 Windows 最底层的 Win32 API 开始，一路讲到 Qt、WinUI、WebView2 等现代技术。你可能会问：为什么不直接学框架？因为只有理解了底层的消息机制、窗口模型、绘制原理，切换任何框架时都会顺畅很多。

获得更好的阅读体验👉: [anatomy_gui](https://charliechen114514.github.io/anatomy_gui/)

---

## 技术覆盖

[![Win32](https://img.shields.io/badge/Win32-API-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![Qt](https://img.shields.io/badge/Qt-Framework-41CD52?style=for-the-badge&logo=qt&logoColor=white)]()
[![WinUI](https://img.shields.io/badge/WinUI-3-0078D4?style=for-the-badge&logo=windows&logoColor=white)]()
[![GTK](https://img.shields.io/badge/GTK-4.0-4DB33D?style=for-the-badge&logo=gtk&logoColor=white)]()
[![WebView2](https://img.shields.io/badge/WebView2-Chromium-00CE7C?style=for-the-badge&logo=chromium&logoColor=white)]()

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

## Win32 阶段进度

当前进度：**65%** | 18个章节已完成

详细的章节目录请查看：[**教程首页 →**](tutorial/index.md)

涵盖：基础篇(4章) | 控件篇(5章) | 对话框篇(3章) | 资源篇(6章)

完整的章节目录请查看：[**教程首页 →**](tutorial/index.md)

涵盖：基础篇(4章) | 控件篇(5章) | 对话框篇(3章) | 资源篇(6章)

### 配套示例代码

每个章节都配有完整的可运行示例，位于 [src/tutorial/native_win32/](src/tutorial/native_win32/) 目录：

- **基础示例**：01_hello_world ~ 10_oop_wrapper（10个渐进式示例）
- **练习项目**：6个实战练习（点击计数器、随机方块、鼠标追踪器、简单记事本、拖动小球、双缓冲）

更多细节请查看 [Roadmap.md](Roadmap.md) 完整规划。

---

## 开发环境

[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022-5C2D91?style=flat-square&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/vs/)
[![Windows SDK](https://img.shields.io/badge/Windows%20SDK-Latest-0078D4?style=flat-square&logo=windows&logoColor=white)](https://developer.microsoft.com/windows/downloads/windows-sdk/)
[![MSVC](https://img.shields.io/badge/MSVC-x64%20%2F%20x86-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)]()
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)

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

---

## License

Copyright © 2024 [Charliechen114514](https://github.com/Charliechen114514)

本项目采用 [MIT License](LICENSE) 开源协议。

---

## 作者

**Charliechen114514**

GitHub: [Charliechen114514](https://github.com/Charliechen114514)

---

> 一直在路上，持续更新中...
