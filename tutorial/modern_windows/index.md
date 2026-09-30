# ModernWindows —— 现代 C++ / 协程 / Win32 GUI 框架剖析

> 用现代 C++（C++20 协程、Concepts、`std::expected`）+ 现代 GUI 思想（细粒度响应式、数据绑定）+ 现代 Win32 栈（Direct2D 1.1 / DirectComposition / DirectWrite），在 Windows 上从零再造一具 GUI 骨架——把“任何一个 GUI 框架藏起来的假设”一个一个摊开给你看。本系列自包含，不预设你读过任何其它教程。

> 没碰过 Win32 也能读：每个 Win32 概念第一次用到时都会就地讲清，不预设你读过任何 Win32 教程。

## 从这里开始

👉 先读 [序章——为什么再造一个 GUI 框架](/modern_windows/00_ModernWindows_序章_为什么再造一个GUI框架)：动机、八器官地图（每个器官落到哪个类、难在哪）、构建架构（CMake + 每个 stage 自包含）、完整 15 阶段路线，一次讲透。

## 已发布的阶段

| 阶段 | 主题 | 器官 |
| :--: | :-- | :-- |
| 00 | [序章——为什么再造一个 GUI 框架](/modern_windows/00_ModernWindows_序章_为什么再造一个GUI框架) | 全局导论 |
| 01 | [器官一·窗口与绘制面](/modern_windows/01a_ModernWindows_工具链与工程骨架)（拆成 01a–01g，完整） | 窗口与绘制面 |
| 02 | [器官二·消息循环与协程](/modern_windows/02_ModernWindows_器官二_消息循环与协程) | 事件循环与输入 |
| 03 | [器官三·控件树与所有权](/modern_windows/03_ModernWindows_器官三_控件树与所有权) | 控件树与所有权 |

> 阶段 04–15 施工中，按“布局 → 绘制 → 对象模型 → 响应式 → 异步 → 文本/IME → 输入 → DPI/主题 → 控件 → 剪贴板 → 无障碍 → 收尾”推进。
