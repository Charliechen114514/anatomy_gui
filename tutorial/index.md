---
layout: home

hero:
  name: 剖析 GUI
  text: 从零造一具骨架
  tagline: 看穿每一个框架的内部设计 · 先解剖 Win32 标本，再手搓 MiniUI 骨架，最后对照 GTK / WinUI 3 / ImGui
  actions:
    - theme: brand
      text: 进入 Act 0 理论篇
      link: /act0-theory/
    - theme: alt
      text: 直奔 Win32 标本解剖
      link: /native_win32/
    - theme: alt
      text: ModernWindows 框架（C++ 协程 + DComp）
      link: /modern_windows/01a_ModernWindows_工具链与工程骨架

features:
  - icon: 🪟
    title: ModernWindows 框架（连载中）
    details: 用现代 C++（C++20 协程、std::expected）+ 现代 Win32 栈（Direct2D 1.1 / DirectComposition）从零造一个 GUI 框架，一器官一篇手搓到底
    link: /modern_windows/01a_ModernWindows_工具链与工程骨架
  - icon: 🩻
    title: Act 0 · GUI 理论篇
    details: 10 篇 · 12 器官世界观 · 保留 vs 即时、命令/声明/响应、事件循环、可访问性、隐藏成本
    link: /act0-theory/
  - icon: 📖
    title: Win32 标本解剖
    details: 61 章 · 从消息机制到 D3D12 · 最透明的一具 GUI 标本，逐个器官开膛
    link: /native_win32/0_ProgrammingGUI_Basic
  - icon: 🦴
    title: MiniUI 骨架手搓
    details: 10 章 · XCB + Cairo · 事件循环 / 控件树 / 布局 / 信号 / 渲染管线 / 协程 / 线程池
    link: /hands_on_ur_own_gui/
  - icon: 🎨
    title: 渲染栈深潜
    details: GDI / GDI+ / Direct2D / DirectWrite / HLSL / Direct3D 11/12 · 从软件光栅到现代图形 API
    link: /native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC
  - icon: 🔬
    title: 比较解剖（Act 2）
    details: GTK / WinUI 3 / ImGui 横向对照 · 同一器官在不同框架里的演化（增量规划中）
    link: /act0-theory/01_the_twelve_organs
  - icon: ⚙️
    title: 产品化与实战
    details: Owner-Draw 自绘控件 / OpenGL 嵌入 / 高 DPI / IME · 把 demo 推向真东西
    link: /native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw
---
