# anatomy_gui · GUI 编程剖析

[![Language](https://img.shields.io/badge/language-C++20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%2B%20Linux-0078D4?style=flat-square&logo=windows&logoColor=white)]()
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> **剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。**

一套面向 C++ 开发者的 GUI 编程**剖析式**教程：先用 Win32 解剖最透明的标本，再亲手造一个 MiniUI 骨架，最后拿这具骨架去对照解剖 GTK / WinUI 3 / ImGui。

📚 在线阅读：[anatomy_gui 教程站](https://charliechen114514.github.io/anatomy_gui/)

---

## 这是什么

市面上要么是零散博客，要么是冷冰冰的官方文档，缺一套能把 GUI 知识体系串起来的**剖析式**教程。本系列不只教「怎么用框架」，而是带你从底层理解每一个 GUI 框架的内部构造——**亲手造一遍，再看任何框架都通透**。

**论点**：所有 GUI 框架都由同一组器官构成——窗口、事件循环、控件树、布局、绘制、对象模型、信号、异步。把这群器官造过一遍，任何框架都只是一组你已熟知的变奏。

---

## 路线总览（六幕 · 顺序与大纲）

| 幕 | 内容 | 角色 | 进度 |
|:---|:---|:---|:---|
| **Act 0 · 准备** | 世界观 + 工具链 | 入口 | 部分已有 |
| **Act 1 · 标本与骨架** | 解剖 Win32 标本 + 造 MiniUI 骨架 | 建立解剖词汇 | 🔄 资产已有，迁移+深化中 |
| **Act 2 · 比较解剖** | 8 器官 × 多标本对照（Win32/GTK/WinUI3/ImGui） | **系列核心** | 🚧 待写 |
| **Act 3 · 渲染栈** | GDI → Direct2D → D3D | 像素如何被制造 | 🔄 资产已有，迁移中 |
| **Act 4 · 成型与交付** | DPI / theming / 性能 / 打包 / i18n / a11y | 把 demo 变产品 | 🚧 待写 |
| **Act 5 · 独立解剖** | Markdown 编辑器（比较版毕业项目） | 闭环 | 🚧 待写 |

> 完整路线与内容迁移说明见 [Roadmap.md](Roadmap.md)；逐幕执行计划见 [todo/](todo/)。

---

## 已有内容（约 72 章，正迁入新结构）

- **Win32 原生编程**（→ Act 1 标本）：基础 / 控件 / 对话框 / 资源 / 进阶消息 / 子类化 / Hook / 托盘 / 拖放 / 定时器
- **图形渲染**（→ Act 3 渲染栈）：GDI → GDI+ → Direct2D / DirectWrite → HLSL → Direct3D 11 → Direct3D 12 → OpenGL
- **MiniUI Handbook**（→ Act 1 骨架）：用 XCB + Cairo 从零手搓一个教学级 GUI 框架（事件循环 / 控件树 / 布局 / 信号 / 渲染管线 / 协程 / 响应式 / 线程池）

---

## 配套代码

[src/tutorial/](src/tutorial/) —— 每章独立可编译示例（CMake + C++20，生成 `compile_commands.json`）。

## 开发环境

- **Windows**：Visual Studio 2022 / MSVC（或 MinGW），Windows SDK，CMake 3.20+
- **Linux**（MiniUI / GTK）：`libxcb1-dev libcairo2-dev`，WSL2 + XLaunch / WSLg
- **构建**：
  ```bash
  cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  cmake --build build --config Release
  ```

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。规划与执行清单见 [todo/](todo/)。

## 许可证

[MIT](LICENSE) · Copyright © 2024-2026 [Charliechen114514](https://github.com/Charliechen114514)

---

> 一直在路上，持续剖析中。
