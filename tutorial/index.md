---
layout: home

hero:
  name: 剖析 GUI
  text: 从零造一具骨架
  tagline: 正在重构 —— 先脱掉封装，用 Win32 徒手写一遍，再把封装一层层长出来，最后落回同一个 demo
  actions:
    - theme: brand
      text: 进入 Act 0 理论篇
      link: /act0-theory/
    - theme: alt
      text: ModernWindows 框架（C++ 协程 + DComp）
      link: /modern_windows/

features:
  - icon: 🩻
    title: Act 0 · GUI 理论篇
    details: 10 篇 · 12 器官世界观 · 保留 vs 即时、命令/声明/响应、事件循环、可访问性、隐藏成本
    link: /act0-theory/
  - icon: 🪟
    title: ModernWindows 框架（连载中）
    details: 用现代 C++（C++20 协程、std::expected）+ 现代 Win32 栈（Direct2D 1.1 / DirectComposition）从零造一个 GUI 框架
    link: /modern_windows/
  - icon: 🦴
    title: 新骨架（施工中）
    details: 具象 → 抽象 → 落回具象：一个能点、能输入、能缩放的骨架，每个器官都说得清它解决了哪处痛点
---