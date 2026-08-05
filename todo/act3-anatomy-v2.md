# 草稿 03 · Anatomy GUI v2 手搓框架

> 目标：允许旧 MiniUI 回退，把“手搓 GUI 框架”重新做成全书的实验平台。v2 不追求工业可用，而追求教学可解释、结构完整、能验证理论，并能与 Win32 / GTK / Qt / WinUI3 / ImGui 对照。

---

## 一、v2 的定位

Anatomy GUI v2 应该是一具“教学骨架”，不是生产级框架。

它应该做到：

- 小到能读完。
- 完整到覆盖 GUI 的主要器官。
- 每个器官都能对应一章理论和一个工业框架对照。
- 每个 stage 都有可运行结果。
- 可以替换平台层或渲染层，证明“框架器官”和“系统 API”不是一回事。

它不应该承诺：

- 生产级性能。
- 完整控件库。
- 跨平台包治百病。
- 复杂 CSS / Flexbox / GPU scene graph 一步到位。

---

## 二、主后端决策（2026-06-18 更新）

> **决定：v2 不换后端、不从零重做。** 继续 XCB + Cairo 主线，直接建在现有 MiniUI（hands_on 62–71）之上；Win32 + D2D 降为「补充接入说明」。

### 主线：XCB + Cairo（延续 MiniUI v1）

理由：

- 复用现有 10 章 + `src/.../stage1-10` 全套可编译资产（CI 已绿）。
- 避免重做框架的巨大工作量，把精力留给内容深化。
- XCB + Cairo 足以讲清窗口系统、事件循环、控件树、布局、绘制、信号等核心器官。

### 补充：Win32 + Direct2D / DirectWrite 作为「后端接入说明」

定位：**不作为主线重做**，而是用 1–2 章演示框架 platform/render 抽象层的可替换性——把同一框架接到 Win32 + D2D，证明「器官」与「系统 API」解耦。

价值：

- 与「现代 Win32」篇闭环，讲清 DWM / IME / UIA 等 Windows 特有器官。
- 不背「重做整个框架」的代价。

### v2 的真正增量 = MiniUI v1 的深化

v2 不是新框架，而是补齐 v1 留下的 deferred 缺口：theming、Per-monitor DPI、GPU/硬件后端、a11y 对应物、IME、DevTools（Inspector / PaintProfiler），再加一章 Win32+D2D 后端接入说明。

> 注：下文 Stage 0–15 路线为早期「Win32+D2D 主线」时期的草稿，按 2026-06-18 决策（主线改回 XCB+Cairo）待重排为「v1 深化 + 后端抽象」形态，现作参考保留。

### 可选未来后端（远期）

- Skia / Wayland / WebCanvas：远期参考，不在第一阶段投入。

---

## 三、核心架构草图

建议命名保持朴素，避免过度抽象。

```text
anatomy_gui/
├─ core/
│  ├─ Application
│  ├─ EventLoop
│  ├─ Window
│  ├─ Widget
│  ├─ Event
│  ├─ Layout
│  ├─ Style
│  ├─ Signal
│  └─ Task
├─ platform/
│  ├─ win32/
│  └─ xcb/
├─ render/
│  ├─ d2d/
│  └─ cairo/
├─ widgets/
│  ├─ Button
│  ├─ Label
│  ├─ TextBox
│  ├─ ScrollView
│  └─ ListView
└─ devtools/
   ├─ Inspector
   ├─ EventLog
   └─ PaintProfiler
```

核心接口可以围绕这些概念展开：

- `Application`：拥有事件循环和全局服务。
- `Window`：平台窗口和根控件的桥。
- `Surface`：平台显示面。
- `Renderer` / `Painter`：绘制抽象。
- `Widget`：控件树节点。
- `Event`：归一化输入和系统事件。
- `LayoutNode`：测量与排布。
- `Style`：主题、字体、颜色、状态。
- `Signal` / `Property`：状态变化。
- `Task` / `Dispatcher`：异步回投。

---

## 四、建议 Stage 路线

### Stage 0 · 框架宣言与最小仓库

产出：

- 目录结构。
- coding style。
- 最小 CMake。
- “框架不是控件库”的说明。

验证：

- 空程序能启动并退出。

### Stage 1 · 平台窗口与事件循环

内容：

- Win32 创建窗口。
- `GetMessage` loop。
- 平台消息进入框架。
- `Application::run()`。

验证：

- 打开一个空白窗口。
- 记录 `create / resize / close` 事件。

### Stage 2 · 资源与生命周期

内容：

- RAII handle。
- `Window` 对象与 `HWND` 绑定。
- 销毁顺序。
- 事件晚到。

验证：

- 多窗口创建关闭。
- debug log 无悬空访问。

### Stage 3 · 归一化事件系统

内容：

- mouse move/down/up/wheel。
- keyboard。
- resize。
- timer。
- event bubbling / capturing 的取舍。

验证：

- EventLog 面板显示事件。

### Stage 4 · 控件树

内容：

- `Widget` parent / children。
- bounds。
- invalidation。
- root widget。
- ownership：unique_ptr、intrusive tree 或 arena 的选择。

验证：

- 树中放三个矩形控件，并打印树。

### Stage 5 · 命中测试、焦点与鼠标捕获

内容：

- hit test。
- hover / active / focus。
- mouse capture。
- tab focus。

验证：

- Button 能 hover、press、release。

### Stage 6 · 布局系统

内容：

- `measure(available)`。
- `arrange(rect)`。
- desired size。
- stack、grid、dock 的最小版本。
- layout invalidation。

验证：

- 同一个表单在窗口 resize 时稳定布局。

### Stage 7 · 绘制抽象与脏区

内容：

- `Painter` 接口。
- Direct2D 后端。
- dirty region。
- double buffering / swap。
- clips。

验证：

- 只重绘被 invalid 的区域，并可视化脏区。

### Stage 8 · 文本系统

内容：

- DirectWrite font。
- text layout。
- caret。
- selection。
- IME composition 的最小接入。

验证：

- TextBox 能输入中文，显示 caret 和 selection。

### Stage 9 · 基础控件库

内容：

- Label。
- Button。
- TextBox。
- CheckBox。
- Slider。
- ScrollView。
- ListView 最小虚拟化。

验证：

- 一个设置面板 demo。

### Stage 10 · 样式与主题

内容：

- color token。
- typography token。
- state style。
- dark mode。
- high contrast。
- resource lookup。

验证：

- 系统主题变化后 UI 自动切换。

### Stage 11 · Signal / Property / Binding

内容：

- `Property<T>`。
- `Signal<Args...>`。
- one-way binding。
- computed property。
- 避免循环更新。

验证：

- MVVM 风格计数器和表单。

### Stage 12 · 协程与异步任务

内容：

- UI dispatcher。
- background executor。
- `co_await resume_ui()`。
- cancellation。
- lifetime guard。

验证：

- 异步加载文件，窗口关闭时安全取消。

### Stage 13 · 可访问性、DPI 与输入法补完

内容：

- role/name/value/state。
- UIA provider 最小实验。
- Per-monitor DPI。
- font fallback。
- IME polish。

验证：

- Inspect.exe 能看到按钮和文本框语义。

### Stage 14 · DevTools：框架自我解剖

内容：

- Widget tree inspector。
- Layout bounds overlay。
- Event log。
- Paint profiler。
- Dirty region overlay。

验证：

- 框架能展示自己的内部器官。

### Stage 15 · Mini App

候选：

- Notes。
- Markdown editor。
- Settings panel。
- File browser。

要求：

- 至少 5 个控件。
- 异步加载。
- 中文输入。
- DPI。
- dark mode。
- 可访问性最小支持。

---

## 五、文档写法建议

v2 可以采用“工程教程 + 设计日志”混合写法：

- 每个 stage 给可编译代码。
- 每章先讲设计选择，再讲代码结构。
- 保留“反设计”小节：为什么不这么做。
- 每章结尾写“映射到工业框架”：Win32 / Qt / GTK / WinUI3 / ImGui 怎么处理同一问题。

这与旧 MiniUI “启发式手册不给代码”的路线不同。建议这样分工：

- 旧 MiniUI v1：继续作为启发式手册。
- Anatomy GUI v2：作为正式可运行实验平台，给代码。

---

## 六、关键设计取舍

### 1. Retained mode 作为主模型

理由：

- 更适合桌面应用、可访问性、布局、输入法、复杂控件。
- 能与 Win32 / GTK / Qt / WinUI3 对照。

同时保留一章即时模式对照：

- 用 ImGui 思路重写同一 panel。
- 说明状态所有权差异。

### 2. 不一开始做 CSS

先做 token + selector-lite：

- color。
- font。
- spacing。
- control state。

高级 CSS cascade 可以作为扩展实验。

### 3. 不一开始做完整 Flexbox

先做：

- Stack。
- Grid。
- Dock。
- Absolute。

再用一章解释 Flexbox / Auto Layout / constraint solver。

### 4. 文本输入必须进入主线

很多玩具 GUI 避开文本，因此避开了 GUI 最难的部分。v2 应至少支持：

- Unicode。
- IME composition。
- caret。
- selection。
- copy/paste。

### 5. 可访问性不是最后贴补丁

每个 widget 从设计时就有：

- role。
- name。
- state。
- bounds。
- action。

即使 UIA provider 后写，内部语义也要先存在。

---

## 七、与旧 MiniUI 的关系

旧 MiniUI 可以保留这些价值：

- XCB 和 Cairo 作为平台/渲染后端对照。
- Observable、线程池、协程章节作为素材。
- 原有“骨架”隐喻继续沿用。

但 v2 可以重新开始：

- 不继承旧命名。
- 不要求兼容旧 stage。
- 不把旧文章中的代码漂移带进新框架。

建议写一章：

> 从 MiniUI v1 到 Anatomy GUI v2：哪些留下，哪些重做。

---

## 八、验收标准

- 能运行一个多控件桌面程序。
- 事件、布局、绘制、样式、异步、文本、DPI、a11y 都至少有最小实现。
- DevTools 能展示控件树、布局框、事件和脏区。
- 至少一个 demo 支持中文输入和高 DPI。
- 至少一个 demo 展示协程异步加载与安全取消。
