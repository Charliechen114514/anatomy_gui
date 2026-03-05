# GUI 通用编程指南

[![Platform](https://img.shields.io/badge/platform-Windows-0078D4?style=flat-square&logo=windows&logoColor=white)](https://learn.microsoft.com/zh-cn/windows/win32/)
[![Language](https://img.shields.io/badge/language-C++-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![IDE](https://img.shields.io/badge/IDE-Visual%20Studio-5C2D91?style=flat-square&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Progress](https://img.shields.io/badge/Win32-36%25-orange?style=flat-square)](Roadmap.md)
[![Writing](https://img.shields.io/badge/Writing-In%20Progress-brightgreen?style=flat-square)](tutorial/native_win32/)
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> 一套面向 C++ 开发者的图形界面编程完整路线图，从 Win32 原生 API 到现代跨平台框架，循序渐进，深度剖析。

---

## 为什么要做这个

说实话，现在想系统地学习 GUI 编程并不容易。市面上要么是零散的博客文章，要么是官方文档那种冷冰冰的参考手册，缺乏一套能把整个知识体系串起来的教程。很多人想学客户端开发，但不知道从哪入手——Electron 太重、Qt 文档太厚、Win32 看起来像上古遗迹。

这个项目的目的就是填补这个空白。我们从 Windows 最底层的 Win32 API 开始，一路讲到 Qt、WinUI、WebView2 等现代技术。你可能会问：为什么不直接学框架？因为只有理解了底层的消息机制、窗口模型、绘制原理，切换任何框架时都会顺畅很多。

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
第一篇  Win32 原生编程      地基（进行中 ████░░░░ 36%）
第二篇  GUI 核心概念        跨框架通识
第三篇  图形渲染            GDI → Direct2D → GPU
第四篇  跨平台框架          Qt / wxWidgets / GTK
第五篇  现代 Windows 技术   WinUI 3 / WebView2
第六篇  Web 混合方案        Electron / CEF / Tauri
第七篇  工程化专题          多线程、DPI、打包、i18n
第八篇  综合实战项目        贯穿全系列的收尾
```

---

## Win32 阶段进度

[![Section 1.1](https://img.shields.io/badge/1.1-程序本质-brightgreen?style=flat)](tutorial/native_win32/0_ProgrammingGUI_Basic.md)
[![Section 1.2](https://img.shields.io/badge/1.2-消息机制-brightgreen?style=flat)](tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md)
[![Section 1.3](https://img.shields.io/badge/1.3-系统消息-brightgreen?style=flat)](tutorial/native_win32/2_ProgrammingGUI_NativeWindows_2.md)
[![Section DPI](https://img.shields.io/badge/DPI-适配专题-brightgreen?style=flat)](tutorial/native_win32/3_ProgrammingGUI_WhatAboutDPI.md)
[![Section 1.4](https://img.shields.io/badge/1.4-标准控件-yellow?style=flat)](tutorial/native_win32/4_ProgrammingGUI_NativeWindows_Controls.md)
[![Section 1.5](https://img.shields.io/badge/1.5-布局资源-lightgrey?style=flat)](Roadmap.md)
[![Section 1.6](https://img.shields.io/badge/1.6-GDI绘图-lightgrey?style=flat)](Roadmap.md)
[![Section 1.7](https://img.shields.io/badge/1.7-Win32进阶-lightgrey?style=flat)](Roadmap.md)

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 1.1 | Windows 程序的本质 | 已完成 | [0_ProgrammingGUI_Basic.md](tutorial/native_win32/0_ProgrammingGUI_Basic.md) |
| 1.2 | 消息机制 | 已完成 | [1_ProgrammingGUI_NativeWindows.md](tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md) |
| 1.3 | 常用系统消息 | 已完成 | [2_ProgrammingGUI_NativeWindows_2.md](tutorial/native_win32/2_ProgrammingGUI_NativeWindows_2.md) |
| 1.3补充 | DPI 适配专题 | 已完成 | [3_ProgrammingGUI_WhatAboutDPI.md](tutorial/native_win32/3_ProgrammingGUI_WhatAboutDPI.md) |
| 1.4 | 标准控件 | 进行中 | [4_ProgrammingGUI_NativeWindows_Controls.md](tutorial/native_win32/4_ProgrammingGUI_NativeWindows_Controls.md) |
| 1.5 | 窗口布局与资源 | 待完成 | — |
| 1.6 | GDI 基础绘图 | 待完成 | — |
| 1.7 | Win32 进阶 | 待完成 | — |

更多细节请查看 [Roadmap.md](Roadmap.md) 完整规划。

---

## 开发环境

[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022-5C2D91?style=flat-square&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/vs/)
[![Windows SDK](https://img.shields.io/badge/Windows%20SDK-Latest-0078D4?style=flat-square&logo=windows&logoColor=white)](https://developer.microsoft.com/windows/downloads/windows-sdk/)
[![MSVC](https://img.shields.io/badge/MSVC-x64%20%2F%20x86-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)]()

- **IDE**: Visual Studio 2022 Community（免费）
- **SDK**: Windows SDK（随 VS 自动安装）
- **编译器**: MSVC（支持 32/64 位）

安装 VS 时勾选「使用 C++ 的桌面开发」工作负载即可。

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
