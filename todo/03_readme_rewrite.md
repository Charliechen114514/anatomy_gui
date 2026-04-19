# Phase 3 — README 与教程首页重写

## 3.1 README.md 全面重写

**文件**：`README.md`

**完整替换为以下内容**：

```markdown
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
```

---

## 3.2 教程首页 index.md 更新

**文件**：`tutorial/index.md`

**在现有内容基础上，在 "资源篇" 表格之后添加以下表格**：

```markdown
### 工具栏与状态栏

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 17.5 | 工具栏与状态栏 | 已完成 | [17_5_ProgrammingGUI_NativeWindows_ToolbarStatusBar.md](native_win32/17_5_ProgrammingGUI_NativeWindows_ToolbarStatusBar.md) |

### GDI 图形篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 18 | GDI 设备上下文（HDC）完全指南 | 已完成 | [18_ProgrammingGUI_NativeWindows_GDI_HDC.md](native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC.md) |
| 19 | GDI 绘图对象：画笔、画刷、字体 | 已完成 | [19_ProgrammingGUI_NativeWindows_GDI_Objects.md](native_win32/19_ProgrammingGUI_NativeWindows_GDI_Objects.md) |
| 20 | GDI 图形绘制：线条、矩形与多边形 | 已完成 | [20_ProgrammingGUI_NativeWindows_GDI_Shapes.md](native_win32/20_ProgrammingGUI_NativeWindows_GDI_Shapes.md) |
| 21 | GDI 文字渲染 | 已完成 | [21_ProgrammingGUI_NativeWindows_GDI_Text.md](native_win32/21_ProgrammingGUI_NativeWindows_GDI_Text.md) |
| 22 | GDI 位图操作 | 已完成 | [22_ProgrammingGUI_NativeWindows_GDI_Bitmap.md](native_win32/22_ProgrammingGUI_NativeWindows_GDI_Bitmap.md) |
| 23 | GDI 双缓冲技术 | 已完成 | [23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md](native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md) |
| 24 | GDI Region 与裁切 | 已完成 | [24_ProgrammingGUI_Graphics_GDI_Region.md](native_win32/24_ProgrammingGUI_Graphics_GDI_Region.md) |
| 25 | Alpha 混合与透明效果 | 已完成 | [25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md](native_win32/25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md) |

### GDI+ 篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 26 | GDI+ 架构与抗锯齿渐变 | 已完成 | [26_ProgrammingGUI_Graphics_GdiPlus_Architecture.md](native_win32/26_ProgrammingGUI_Graphics_GdiPlus_Architecture.md) |
| 27 | 坐标变换与矩阵 | 已完成 | [27_ProgrammingGUI_Graphics_GdiPlus_Transform.md](native_win32/27_ProgrammingGUI_Graphics_GdiPlus_Transform.md) |
| 28 | 图像格式与编解码 | 已完成 | [28_ProgrammingGUI_Graphics_GdiPlus_ImageCodec.md](native_win32/28_ProgrammingGUI_Graphics_GdiPlus_ImageCodec.md) |

### Direct2D / DirectWrite 篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 29 | Direct2D 架构与资源体系 | 已完成 | [29_ProgrammingGUI_Graphics_Direct2D_Architecture.md](native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture.md) |
| 30 | Direct2D 几何体系统 | 已完成 | [30_ProgrammingGUI_Graphics_Direct2D_Geometry.md](native_win32/30_ProgrammingGUI_Graphics_Direct2D_Geometry.md) |
| 31 | Direct2D 效果与图层 | 已完成 | [31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md](native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md) |
| 32 | DirectWrite 高质量文字排版 | 已完成 | [32_ProgrammingGUI_Graphics_DirectWrite_Typography.md](native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography.md) |
| 33 | Direct2D 与 Win32/GDI 互操作 | 已完成 | [33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md](native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md) |

### HLSL 着色器篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 34 | HLSL 语言基础 | 已完成 | [34_ProgrammingGUI_Graphics_HLSL_Basics.md](native_win32/34_ProgrammingGUI_Graphics_HLSL_Basics.md) |
| 35 | HLSL 编译与调试 | 已完成 | [35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md](native_win32/35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md) |
| 36 | Constant Buffer 与数据传递 | 已完成 | [36_ProgrammingGUI_Graphics_HLSL_CBuffer.md](native_win32/36_ProgrammingGUI_Graphics_HLSL_CBuffer.md) |

### Direct3D 11 篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 37 | D3D11 初始化与 SwapChain | 已完成 | [37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md](native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md) |
| 38 | 顶点缓冲与输入布局 | 已完成 | [38_ProgrammingGUI_Graphics_D3D11_VertexInput.md](native_win32/38_ProgrammingGUI_Graphics_D3D11_VertexInput.md) |
| 39 | 纹理与采样器 | 已完成 | [39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md](native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md) |
| 40 | 深度缓冲与 3D 变换 | 已完成 | [40_ProgrammingGUI_Graphics_D3D11_Depth3D.md](native_win32/40_ProgrammingGUI_Graphics_D3D11_Depth3D.md) |
| 41 | 光照模型基础 | 已完成 | [41_ProgrammingGUI_Graphics_D3D11_Lighting.md](native_win32/41_ProgrammingGUI_Graphics_D3D11_Lighting.md) |
| 42 | 混合与透明渲染 | 已完成 | [42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md](native_win32/42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md) |

### Direct3D 12 篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 43 | D3D12 设计哲学 | 已完成 | [43_ProgrammingGUI_Graphics_D3D12_Philosophy.md](native_win32/43_ProgrammingGUI_Graphics_D3D12_Philosophy.md) |
| 44 | 命令列表、队列与围栏 | 已完成 | [44_ProgrammingGUI_Graphics_D3D12_CmdQueue.md](native_win32/44_ProgrammingGUI_Graphics_D3D12_CmdQueue.md) |
| 45 | 资源与堆管理 | 已完成 | [45_ProgrammingGUI_Graphics_D3D12_ResourceHeap.md](native_win32/45_ProgrammingGUI_Graphics_D3D12_ResourceHeap.md) |
| 46 | 描述符堆与根签名 | 已完成 | [46_ProgrammingGUI_Graphics_D3D12_DescRootSig.md](native_win32/46_ProgrammingGUI_Graphics_D3D12_DescRootSig.md) |
| 47 | D3D12 与 D3D11 互操作 | 已完成 | [47_ProgrammingGUI_Graphics_D3D12_D3D11Interop.md](native_win32/47_ProgrammingGUI_Graphics_D3D12_D3D11Interop.md) |

### 自定义控件篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 48 | Owner-Draw 控件 | 已完成 | [48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw.md](native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw.md) |
| 49 | 完全自绘控件架构 | 已完成 | [49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md](native_win32/49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md) |
| 50 | 命中测试与鼠标事件路由 | 已完成 | [50_ProgrammingGUI_Graphics_CustomCtrl_HitTest.md](native_win32/50_ProgrammingGUI_Graphics_CustomCtrl_HitTest.md) |

### OpenGL 篇

| 章节 | 内容 | 状态 | 文件 |
|:-----|:-----|:-----|:-----|
| 51 | Win32 嵌入 OpenGL | 已完成 | [51_ProgrammingGUI_Graphics_OpenGL_Win32.md](native_win32/51_ProgrammingGUI_Graphics_OpenGL_Win32.md) |
| 52 | Qt QOpenGLWidget 封装 | 已完成 | [52_ProgrammingGUI_Graphics_OpenGL_QtGLWidget.md](native_win32/52_ProgrammingGUI_Graphics_OpenGL_QtGLWidget.md) |
```

> 同时更新 index.md 中的 "第一篇 Win32 原生编程" 标题为 "Win32 原生编程 + 图形渲染"，以反映实际内容范围。

---

## 验证

- README 数据与实际文件数一致（19 章 Win32 + 35 章图形渲染 = 54 章）
- index.md 所有链接可跳转到对应文件
- `mkdocs build --clean` 构建无错误
