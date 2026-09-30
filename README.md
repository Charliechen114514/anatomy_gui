# anatomy_gui · GUI 编程剖析

[![Language](https://img.shields.io/badge/language-C++20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%2B%20Linux-0078D4?style=flat-square&logo=windows&logoColor=white)]()
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

> **剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。**

一个面向 C++ 开发者的 GUI 编程**解剖式**教程。

🔗 在线阅读：[anatomy_gui 教程站](https://charliechen114514.github.io/anatomy_gui/)

> ⚠ **仓库正在重构**：旧结构（Win32 篇 63 章 + MiniUI 篇 10 章）与配套示例已移入私有的 `intern/legacy/`（该目录被 gitignore，不对外发布），站点上相应内容已下线。新骨架的路线见 [`todo/route.md`](todo/route.md)。

---

## 论点

所有 GUI 框架，无论多"魔法"，都由同一组器官构成——窗口、事件循环、输入、对象模型、控件树、布局、绘制、文本、样式、状态绑定、异步、可访问性。亲手把这组器官造一遍，就能把**任何**框架读成一组你已经熟知的变奏。

---

## 重构后的路线

**具象 → 抽象 → 落回具象**

| 步 | 内容 | 状态 |
|:--:|:---|:---|
| 0 | 清场：旧结构归档、站点与 CI 断引用 | ✅ 已完成 |
| 1 | **具象**：裸 Win32 写一遍，故意不做封装 | ⬜ |
| 2 | **抽象**：五个痛点 → 五层封装 | ⬜ |
| 3 | **落回具象**：用框架重写同一个 demo | ⬜ |
| 4 | M1 收口：`tag m1` + "控件如何上屏"专题 | ⬜ |
| 5 | M2：接管轴七档（自绘 / 自管合成 / 文本 / 输入 / 无障碍 / 响应式） | ⬜ |

完整的步骤、验收与红线见 [`todo/route.md`](todo/route.md)；定位与架构见 [`todo/`](todo/)。

---

## 站点上现有内容

- **Act 0 · GUI 理论篇**（10 篇）：12 器官世界观、保留 vs 即时、命令 / 声明 / 响应式、事件循环、可访问性、隐藏成本
- **ModernWindows**（连载中）：用现代 C++ + 现代 Win32 栈，从零造一具骨架

## 构建

```bash
# 站点
pnpm install
pnpm dev            # 本地预览；pnpm build 构建产物

# 代码示例（ModernWindows，Windows + MSVC）
cmake -B build -S src/tutorial/modern_windows
cmake --build build --config Release
```

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE) · Copyright © 2024-2026 [Charliechen114514](https://github.com/Charliechen114514)