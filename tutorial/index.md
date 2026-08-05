---
layout: home

hero:
  name: anatomy_gui
  text: GUI 编程全栈教程
  tagline: 从 Win32 原生 API 到现代跨平台框架，循序渐进，深度剖析
  actions:
    - theme: brand
      text: 开始学习 Win32
      link: /native_win32/0_ProgrammingGUI_Basic
    - theme: alt
      text: ModernWindows 框架（C++ 协程 + DComp）
      link: /modern_windows/01a_ModernWindows_工具链与工程骨架

features:
  - title: ModernWindows 框架（连载中）
    details: 用现代 C++（C++20 协程、std::expected）+ 现代 Win32 栈（Direct2D 1.1 / DirectComposition）从零造一个 GUI 框架，一器官一篇手搓到底
    link: /modern_windows/01a_ModernWindows_工具链与工程骨架
  - title: Win32 原生编程
    details: 61 章 · 从消息机制到 D3D12 · 覆盖 GUI 编程全部核心知识
    link: /native_win32/0_ProgrammingGUI_Basic
  - title: GDI / GDI+ 图形
    details: 设备上下文、画笔画刷、抗锯齿、渐变、矩阵变换、图像编解码
    link: /native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC
  - title: Direct2D / DirectWrite
    details: 硬件加速渲染、几何体、效果图层、高质量文字排版、GDI 互操作
    link: /native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture
  - title: Direct3D 11/12
    details: HLSL 着色器、3D 渲染管线、命令队列、根签名、D3D11 互操作
    link: /native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain
  - title: 自定义控件
    details: Owner-Draw、完全自绘、命中测试与鼠标事件路由
    link: /native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw
  - title: OpenGL / Win32
    details: Win32 窗口嵌入 OpenGL 渲染上下文
    link: /native_win32/51_ProgrammingGUI_Graphics_OpenGL_Win32
  - title: 补充专题
    details: 主题对话框纹理、TabControl 等零散但实用的进阶技巧
    link: /bonus/EnableThemeDialogTexture_TabControl
---
