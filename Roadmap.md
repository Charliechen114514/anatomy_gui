# GUI 编程剖析 · 总路线图（anatomy edition）

> **定位（2026-06 重定位）**：剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。
> 本文件是新路线与内容迁移的权威依据；执行细节在 `todo/`。
> 旧的「按技术分篇」路线图（1504 行）保留在 git 历史 `HEAD:Roadmap.md`，供回溯。

---

## 一、定位

**论点**：所有 GUI 框架，无论多「魔法」，都由同一组器官构成——窗口、事件循环、控件树、布局、绘制、对象模型、信号、异步。亲手把这组器官造一遍，就能把**任何**框架读成一组你已经熟知的变奏。

**方法（解剖学隐喻，是品牌资产）**

| 隐喻 | 对应 | 角色 |
|:---|:---|:---|
| 最透明的标本（青蛙） | **Win32** | 每条消息、每个 HWND、每次绘制都肉眼可见——先解剖它，建立词汇 |
| 亲手拼的骨架 | **MiniUI** | 用最少代码重建每个器官，证明你真懂了 |
| 工业标本 | **GTK / WinUI3 / ImGui** | 拿骨架去对照，看同一器官在工业实现里长成什么样 |

**读者蜕变**：从「会用一个框架」→「能看穿任何框架的内部设计 · 新框架一周上手 · 自己能造一个」。

**反定位（我们不是什么）**
- ❌ 不是框架用法指南（那是官方文档的场子）
- ❌ 不追求广度覆盖（不学全 Qt+GTK+WinUI3+…）
- ❌ 不是版本 API 手册
- ❌ 不服务「只想快点出 CRUD」的人（那是 Tauri/Electron）

**Tagline**：「剖析 GUI —— 从零造一具骨架，看穿每一个框架的内部设计。」

---

## 二、路线总览（六幕）

| 幕 | 内容目录 | 角色 | 状态 |
|:---|:---|:---|:---|
| **Act 0 · 准备** | `tutorial/act0-prep/` | 世界观 + 工具链 | 部分已有（第〇篇） |
| **Act 1 · 标本与骨架** | `tutorial/act1-specimen/` | 解剖 Win32 标本 + 拼 MiniUI 骨架（建立解剖词汇） | 资产已有，待迁移+深化 |
| **Act 2 · 比较解剖** | `tutorial/act2-dissect/` | 8 器官 × 多标本对照（系列核心） | 全新，待写 |
| **Act 3 · 渲染栈** | `tutorial/act3-render/` | 显微镜下的像素：GDI → Direct2D → D3D | 资产已有，待迁移 |
| **Act 4 · 成型与交付** | `tutorial/act4-ship/` | 让标本活下来：DPI / theming / 性能 / 打包 / i18n | 待写（精简） |
| **Act 5 · 独立解剖** | `tutorial/act5-capstone/` | 比较版毕业项目 | 待写 |

---

## 三、仓库架构（保持简单）

**顶层不变**：`README.md` / `Roadmap.md`（本文件）/ `CONTRIBUTING.md` / `tutorial/` / `src/` / `todo/` / `site/`。

**`tutorial/`** —— 内容，按 act 分目录（取代旧的 `native_win32/` + `hands_on_ur_own_gui/` 平铺）：
```
tutorial/
├─ act0-prep/                 世界观、工具链、怎么读官方文档
├─ act1-specimen/             标本与骨架
│  ├─ win32/                     解剖最透明的标本（← 现 native_win32 的 Win32 核心）
│  └─ miniui/                    亲手拼骨架（← 现 hands_on_ur_own_gui/handbook）
├─ act2-dissect/              比较解剖（核心）
│  ├─ organs/                    8 个「器官比较解剖」章
│  └─ specimens/                 标本实操：gtk4 / winui3 / imgui（精简）
├─ act3-render/               渲染栈（← 现 graphics）
├─ act4-ship/                 工程化（精简）
├─ act5-capstone/             比较版毕业项目
├─ reference/                 附录：GTK3 差异、系统 API、被精简的工具书内容
└─ images/
```

**`src/`** —— 源码不变其职，仅镜像 act 结构：`src/tutorial/actN-…/<example>/`。迁移时跟随 `tutorial/`。

**`todo/`** —— 规划，见 [`todo/README.md`](todo/README.md)。

> ⚠️ 站点用 VitePress（`site/.vitepress/`，`srcDir=../tutorial`）。**目录重构必须同步改 `sidebar.ts` 的扫描逻辑**（当前只扫 `native_win32`/`bonus`，且分组 prefix 有 bug——见 [`todo/optimize_site.md`](todo/optimize_site.md) T1/T2）。

---

## 四、逐幕路线（写什么 / 做出什么）

### Act 0 · 准备
迁移现有第〇篇：为什么要学 GUI、GUI 四要素、工具链（MSVC/MinGW/CMake）、怎么读 MSDN。
**做出**：能跑起来的 `hello_window` + 一份可复用 CMake 模板。

### Act 1 · 标本与骨架（建立解剖词汇）
两条线，互为参照：

- **`win32/`（标本）**——迁移现有 Win32 核心（ch0–17 基础/控件/对话框/资源 + ch52–60 进阶消息/子类化/Hook/托盘/拖放/定时器）。每个主题以「Win32 如何实现这个器官」的视角写。**顺手补复查发现的现代 Win32 缺口**：沉浸式深色模式/DWM、应用清单 Manifest、IFileDialog、Shell API（详见 [`todo/act-1-specimen.md`](todo/act-1-specimen.md)）。
- **`miniui/`（骨架）**——迁移现有 handbook（ch62–71：XCB 窗口→事件循环→控件树→布局→信号→渲染管线→协程→响应式→线程池），并**深化**三个 deferred 缺口：theming、DPI、一个 Direct2D/GPU 后端。最后用「大门」章（原 ch72）把每个 MiniUI 器官映射到工业框架，作为通往 Act 2 的桥。

> Act 1 完成后，读者手里有一具完整骨架 + 一套解剖词汇。

### Act 2 · 比较解剖（系列核心，全新）
**8 个「器官比较解剖」章**——每章先回顾 MiniUI 怎么造这个器官，再解剖 Win32 / GTK / WinUI3 / ImGui 各自的工业实现：

| # | 器官 | 骨架(MiniUI) | 工业标本对照 |
|:---:|:---|:---|:---|
| 1 | 窗口与显示面 | XCB window | HWND / GtkWindow / ContentControl+visual / ImGui viewport |
| 2 | 事件循环与输入 | poll+postTask | GetMessage / glib main loop / DispatcherQueue / 每帧轮询 |
| 3 | 控件树与所有权 | Widget 树 | HWND 树 / GtkWidget 树 / DependencyObject 树 / 即时模式「树」幻象 |
| 4 | 布局 | Concepts Layout | MoveWindow / LayoutManager / MeasureOverride / 手动坐标 |
| 5 | 绘制与脏区（保留 vs 即时） | DoubleBuffer+脏区 | HDC+InvalidateRect / Snapshot / Composition / draw list（无脏区） |
| 6 | 对象模型与类型系统 | C++ 类 | 句柄+WNDPROC / **GObject** / **COM·WinRT** / **无** |
| 7 | 信号 / 响应式 / 绑定 | Signal+Property | WM_COMMAND / g_signal / DependencyProperty+{x:Bind} / 返回值 |
| 8 | 异步与并发 | Task+ThreadPool | PostMessage / g_idle_add / IAsyncOperation / 同步（无） |

> **COM 不再是孤立一章**——它住进「对象模型」器官，与 GObject、ImGui 的「无」正面对照（这是最 illuminating 的比较）。

**`specimens/`（标本实操，精简）**：每个框架一篇「上手实操」——只讲工具链/工程结构/idiom（GTK4-only、含 COM、ImGui），不讲已被器官章覆盖的内部原理。
- `gtk4/`、`winui3/`（含 MSIX）、`imgui/`

**砍**：GTK3 双线（差异进 `reference/`）、CEF 独立篇（合并进器官 5/6 对照）、Vulkan 独立篇（→ Act 3 单章对比）。

### Act 3 · 渲染栈（迁移现有 graphics）
迁移 ch18–51、ch61（GDI → GDI+ → Direct2D/DirectWrite → HLSL → D3D11 → D3D12 → 自定义控件 → OpenGL）到 `act3-render/`。**新增 1 章**：Vulkan vs D3D12 概念对比（不系统教 Vulkan）。迁移时修复图形高危 bug（HLSL cbuffer、GDI+ FromFile、字体句柄泄漏等，详见 [`todo/act-3-render.md`](todo/act-3-render.md)「复查待修」节）。

### Act 4 · 成型与交付（精简）
- DPI 适配（Per-Monitor V2）、theming（深色模式/Mica——接复查 H1）、性能（脏区/虚拟列表）、多线程与 UI 线程安全、打包（Inno/MSIX/AppImage/DMG）、i18n。
- **砍**：注册表/系统服务（→ `reference/`）。
- **补**：辅助功能（WM_GETOBJECT/UIA）作跨器官专题（复查 H3 指出 ch48–50 自绘控件缺这条）。

### Act 5 · 独立解剖（比较版毕业项目）
同一份 Markdown 编辑器规格，**2 个完整实现**（Win32+WebView2、GTK/gtkmm）+ **1 篇设计哲学对比文**（覆盖 WinUI3，不再做第 3 个完整实现）。验收线：>10MB 异步加载、多显示器 DPI、中文 IME、未保存确认、干净系统部署。

---

## 五、迁移总表（简表）

| 现有资产 | 新家 | 处理 |
|:---|:---|:---|
| native_win32 ch0–17, 52–60（Win32 核心） | `act1-specimen/win32/` | 迁移 + 补现代 Win32 缺口 |
| native_win32 ch18–51, 61（graphics） | `act3-render/` | 迁移 + 修图形 bug + 加 Vulkan 对比章 |
| native_win32 ch48–50（自定义控件） | `act1-specimen/win32/`（高级） | 迁移；在器官 3/5 解剖时引用 |
| hands_on_ur_own_gui/handbook ch62–71 | `act1-specimen/miniui/` | 迁移 + 深化 theming/DPI/GPU + 修 MiniUI bug |
| `src/tutorial/native_win32/*` | `src/tutorial/act1-specimen/win32/` | 跟随迁移 |
| `src/tutorial/graphics/*` | `src/tutorial/act3-render/` | 跟随迁移 |
| `src/tutorial/anatomy_gui_for_tutorials/stage1-10` | `src/tutorial/act1-specimen/miniui/` | 跟随迁移 |
| 旧 Phase 3 GTK3 部分 | `reference/` | 砍主体，留差异附录 |
| 旧 Phase 5 CEF 独立 2 章 | 并入器官 5/6 | 合并 |
| 旧 Phase 6 Vulkan 独立 2 章 | `act3-render/` 单章 | 压缩 |
| 旧 Phase 7 注册表/系统服务 | `reference/` | 移附录 |

> 完整逐文件迁移清单与执行步骤见 [`todo/migration.md`](todo/migration.md)。

---

## 六、配套清单

- [`todo/content-fixes.md`](todo/content-fixes.md) —— 文字质量修复（错别字/病句/术语/断链）。技术类与结构性问题在各 act 文件的「复查待修」节。
- [`todo/optimize_site.md`](todo/optimize_site.md) —— VitePress 站点优化。**目录重构须同步改 `sidebar.ts`**（T1 prefix bug + 新 act 扫描逻辑）+ 接回 MiniUI（T2）。

---

## 七、写作节奏

```
轨 A（主线）   完成 Act 1 迁移+深化 → Act 2 八器官（核心）→ Act 5 毕业
轨 B（Windows） Act 1 win32 现代缺口 → specimens/winui3（含 COM）
轨 C（图形）    Act 3 渲染栈迁移 + Vulkan 对比章
轨 D（站点）    sidebar.ts 适配新 act 结构 + 接回内容（optimize_site T1/T2/T5）
```
- 先做 Act 1（词汇）→ 再 Act 2（器官，依赖 Act 1 的骨架）。
- Act 3/4 可与 Act 2 并行。
- Act 5 最后（依赖 Act 1/2）。

---

## 八、进度

- ✅ 已有资产（待迁移）：Win32 核心（~31 章）、图形渲染（~31 章）、MiniUI handbook（10 章）≈ 72 章
- 🚧 待写新内容：Act 2 八器官 + 标本实操、Act 1 深化（theming/DPI/GPU）、Act 3 Vulkan 对比章、Act 4 工程精简、Act 5 毕业项目
- 📋 待执行：`todo/` 规划已理顺（见 `todo/README.md`），下一步按幕迁移 + 写新内容

> 一直在路上，持续剖析中。
