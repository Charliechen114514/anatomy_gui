# anatomy_gui · GUI 编程剖析

[![Language](https://img.shields.io/badge/language-C++20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D4?style=flat-square&logo=windows&logoColor=white)]()
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> **剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。**

一个面向 C++ 开发者的 GUI 编程**解剖式**教程。

🔗 在线阅读：[anatomy_gui 教程站](https://charliechen114514.github.io/anatomy_gui/)

> **状态**：主线「造骨架」已完成第一阶段；Win32 旧教程已归位为「Windows 图形基础」书架（63 章，补充用）。MiniUI 那条线已从站点下线，旧内容留在 git 历史里。

---

## 论点

所有 GUI 框架，无论多"魔法"，都由同一组器官构成——窗口、事件循环、输入、对象模型、控件树、布局、绘制、文本、样式、状态绑定、异步、可访问性。亲手把这组器官造一遍，就能把**任何**框架读成一组你已经熟知的变奏。

---

## 路线

**具象 → 抽象 → 落回具象**

| 步 | 内容 | 状态 |
|:--:|:---|:---|
| 0 | 清场：旧结构归档、站点与 CI 断引用 | ✅ 已完成 |
| 1 | **具象**：裸 Win32 写一遍，故意不做封装 | ✅ 已完成 |
| 2 | **抽象**：五个痛点 → 五层封装 | ✅ 已完成 |
| 3 | **落回具象**：用框架重写同一个 demo | ✅ 已完成 |
| 4 | M1 收口：`tag m1` + "控件如何上屏"专题 | ✅ 已完成 |
| 5 | 接管：把器官一个个从系统手里拿回来（自绘 / 合成 / 文本 / 输入 / 无障碍 / 响应式） | ⬜ 进行中 |

完整的步骤、验收与红线见 [`todo/route.md`](todo/route.md)；定位与架构见 [`todo/`](todo/)。

---

## 站点上现有内容

- **造骨架 · 从具象到抽象**（8 篇，第一阶段完成）：裸 Win32 → 五层封装 → 写回同一个程序 → 控件如何上屏
- **Win32 图形基础**（63 章，补充书架）：窗口、消息、控件、对话框、资源、GDI/GDI+、Direct2D、D3D；想补 Windows 图形界面的底子时来翻
- **Act 0 · GUI 理论篇**（10 篇）：12 器官世界观、保留 vs 即时、命令 / 声明 / 响应式、事件循环、可访问性、隐藏成本

## 构建

```bash
# 站点
pnpm install
pnpm dev            # 本地预览；pnpm build 构建产物

# 骨架代码（Windows + clang-cl + Ninja）
cmake -G Ninja -B build -S .
cmake --build build -j
```

Win32 示例是每章独立的小工程，在 `src/tutorial/native_win32/` 下按章构建。

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE) · Copyright © 2024-2026 [Charliechen114514](https://github.com/Charliechen114514)
