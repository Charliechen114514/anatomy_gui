# GUI 编程剖析 · 总路线图（anatomy edition）

> ⚠ **历史文档**：本文描述的是旧结构（八幕 / 72 章），已不作为执行依据。当前执行路线见 [todo/route.md](todo/route.md)。
> 2026-10-29：ModernWindows 已废弃；Win32 旧教程已归位为「Windows 图形基础」书架（63 章）。以下历史描述仅存档。

> **定位**：剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。
> ~~本文件是对外权威路线~~ —— 已作废。当前执行路线见 [`todo/route.md`](todo/route.md)，索引见 [`todo/README.md`](todo/README.md)。

---

## 一、定位

**论点**：所有 GUI 框架，无论多「魔法」，都由同一组器官构成——窗口、事件循环、控件树、布局、绘制、对象模型、信号、异步、文本、样式、可访问性。亲手把这组器官造一遍，就能把**任何**框架读成一组你已经熟知的变奏。

**方法（解剖学隐喻，是品牌资产）**

| 隐喻 | 对应 | 角色 |
|:---|:---|:---|
| 最透明的标本（青蛙） | **Win32** | 每条消息、每个 HWND、每次绘制都肉眼可见——先解剖它，建立词汇 |
| 亲手拼的骨架 | **MiniUI / Anatomy GUI** | 用最少代码重建每个器官，证明你真懂了 |
| 工业标本 | **GTK / Qt / WinUI3 / ImGui / Web / Slint** | 拿骨架去对照，看同一器官在工业实现里长成什么样 |

**读者蜕变**：从「会用一个框架」→「能看穿任何框架的内部设计 · 新框架一周上手 · 自己能造一个」。

**反定位（我们不是什么）**
- ❌ 不是框架用法指南（那是官方文档的场子）
- ❌ 不追求广度覆盖（不学全所有框架）
- ❌ 不是版本 API 手册
- ❌ 不服务「只想快点出 CRUD」的人（那是 Tauri/Electron）

**Tagline**：「剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。」

---

## 二、演进策略：增量挂载（D1）

现有 72 章（Win32 全栈 0–61 + MiniUI 62–71）**冻结为 v1.0 主干 + 标本库**（站点呈现已修、CI stage1-10 全绿）。新内容**逐篇写、逐篇挂进新目录**，绝不回头重组现有章节——保持旧链接 / SEO 不变、永远可发布。

> 战略决策 D1–D4 详见 [`todo/README.md`](todo/README.md)。

---

## 三、路线总览（八幕）

| 幕 | 内容 | 角色 | 状态 |
|:---:|:---|:---|:---|
| Act 0 | GUI 思想史与理论词汇 | 入口：建立「GUI 为什么长成这样」的词汇 | 🟢 增量首选 |
| Act 1 | 传统 Win32 解剖 | 最透明的标本（HWND / 消息泵 / 控件 / GDI） | ✅ 已就绪（主干） |
| Act 2 | 现代 Win32 / C++20 | RAII / COM / DWM / WebView2 / 协程 | 🟢 增量 |
| Act 3 | Anatomy GUI v2 | MiniUI v1 深化（theming/DPI/a11y/IME/DevTools）+ 后端接入 | 🟡 深化 |
| Act 4 | 工业框架比较解剖 | 12 器官 × A 级 5 样本 + 旁证（系列核心） | 🟡 核心待写 |
| Act 5 | 渲染与视觉系统 | GDI→D2D→D3D + 文本 / 合成 / 动画 | ✅ 主干 + 🟢 扩展 |
| Act 6 | 产品化与人因 | a11y / IME / i18n / 测试 / 打包 / 性能 | 🟢 高价值 |
| Act 7 | 毕业项目 | Markdown / Inspector / 文件管理器等多项目 | 🟡 扩展 |

> 逐幕详细大纲见 [`todo/`](todo/README.md) 下 `act0-theory.md` … `act7-capstone.md`。

---

## 四、写作基调：分而治之（D2）

| 板块 | 基调 | 给不给可编译代码 |
|:---|:---|:---|
| Win32 / graphics / 渲染 | 工程教程（概念→代码→坑→作业） | 给，每章配 `src/` 示例 |
| MiniUI / Anatomy GUI v2 | 启发式手册（不给签名 / 骨架） | 不给，逼读者自己造 |

详见 [`CONTRIBUTING.md`](CONTRIBUTING.md)。

---

## 五、推进顺序

**第一批**（改变读者理解方式）：Act0 理论总论（12 器官）→ 保留 vs 即时模式 → 传统→现代 Win32 桥接 → 协程版消息循环 → Anatomy v2 总架构 → v2 五个最小 stage → 同题多框架对照。

**第二批**（真实软件能力）：IME / 文本 / Unicode → DPI / 多显示器 / 高对比 → a11y / UI Automation → 虚拟列表 / 脏区 / 帧调度 → 打包 / 更新 / 崩溃 / 自动化测试。

---

## 六、仓库架构

```
anatomy_gui/
├─ tutorial/        内容（现只剩 Act 0 理论篇 + ModernWindows；旧结构已归档）
├─ src/tutorial/    可编译示例（现只剩 modern_windows）
├─ site/            VitePress 站点
├─ todo/            规划（见 todo/README.md）
├─ Roadmap.md       本文件（对外路线）
├─ README.md        项目门面
└─ CONTRIBUTING.md  贡献规范
```

> D1「不重组」已作废：旧结构整体归档进 `intern/legacy/`，站点上只剩 Act 0 理论篇与 ModernWindows。

---

## 七、进度

> 本节随重构作废。**当前进度以 [`todo/route.md`](todo/route.md) 的总览表为准。**

- 旧结构（Win32 全栈 + MiniUI，72 章）与配套示例已归档进私有的 `intern/legacy/`，已从站点下线
- 站点上现只剩 **Act 0 理论篇** 与 **ModernWindows**（连载中）
- 新骨架按「具象 → 抽象 → 落回具象」推进，**第 0 步（清场）已完成**

> 一直在路上，持续剖析中。