# 草稿 01 · GUI 思想传统与核心理论

> 目标：先让读者理解 GUI 为什么这样设计，再进入 Win32 和手搓框架。这里不是“历史八卦”，而是把传统、范式和工程约束变成后续章节的词汇表。

---

## 一、本板块解决什么问题

现有教程能教“如何创建窗口、处理消息、绘制像素”，但缺少这些问题的总答案：

- 为什么 GUI 必须有事件循环？
- 为什么几乎所有 GUI 都要求 UI 线程亲和？
- 为什么控件树、布局树、视觉树、可访问性树不是一回事？
- 为什么有些框架强调对象身份，有些框架强调每帧重建 UI？
- 为什么声明式 UI 不是“语法糖”，而是一种状态同步模型？
- 为什么输入法、焦点、可访问性、DPI 会改变框架内部设计？
- 为什么 Web、桌面、游戏 UI 看起来不同，但内部问题相通？

建议把这一板块作为新 Act 0：先给读者一张 GUI 世界地图。

---

## 二、GUI 传统谱系

### 1. Smalltalk / Xerox PARC 传统

关键词：窗口、鼠标、控件、MVC、对象身份、消息发送。

可写内容：

- GUI 从“命令行命令”转向“可见对象操作”。
- MVC 最早不是 Web MVC，而是 GUI 中模型、视图、控制器的协作。
- 对象和控件天然绑定：按钮不是函数调用，而是一个有身份、有状态、有生命周期的对象。

对应后文：

- 控件树与对象生命周期。
- 信号、事件、命令模式。
- ViewModel / Binding 的来历。

### 2. Win32 传统

关键词：HWND、消息泵、WNDPROC、资源、GDI、系统控件。

可写内容：

- Win32 是“低层保留模式 GUI”的透明标本。
- `HWND` 是系统托管的对象身份。
- `WM_*` 消息是系统、用户、控件之间的协议。
- `InvalidateRect` 和 `WM_PAINT` 体现了早期 GUI 的脏区重绘模型。

对应后文：

- Act 1 的传统 Win32。
- Act 2 的现代 Win32 封装。
- Act 4 中与 GTK / Qt / WinUI3 的对照。

### 3. X11 / Toolkit 传统

关键词：客户端/服务器、窗口系统、事件队列、Toolkit 分层。

可写内容：

- X11 本身不是完整 GUI 框架，而是窗口系统协议。
- GTK / Qt 在 X11 或 Wayland 上补齐控件、主题、布局、输入法、可访问性。
- XCB + Cairo 手搓框架适合作教学，但现代 Linux 还必须解释 Wayland。

对应后文：

- Anatomy GUI v2 的平台抽象。
- GTK / Qt 作为工业 Toolkit 标本。

### 4. Cocoa / Responder Chain 传统

关键词：run loop、responder chain、target-action、delegate、Auto Layout。

可写内容：

- Apple 系 GUI 强调应用生命周期、响应链和对象协作。
- Responder chain 是焦点和命令路由的另一种表达。
- Auto Layout 展示了约束布局和传统盒布局的不同。

对应后文：

- 焦点、快捷键、菜单命令路由。
- 布局系统比较。
- 作为旁证，不一定成为主教程。

### 5. Web DOM / CSS / React 传统

关键词：DOM 树、CSS 级联、事件冒泡、浏览器渲染管线、虚拟 DOM、组件状态。

可写内容：

- Web 是保留模式树，但开发者常用声明式方式描述它。
- CSS 把样式系统独立成一门语言，值得 GUI 框架借鉴。
- React 的核心不是“网页”，而是状态到 UI 的重复投影。

对应后文：

- 样式系统。
- 声明式 UI 与状态同步。
- WebView2 / 混合桌面应用。

### 6. 游戏 UI / 即时模式传统

关键词：frame loop、draw list、immediate mode、工具 UI、调试面板。

可写内容：

- 游戏 UI 往往跟随每帧刷新，而不是系统脏区回调。
- ImGui 用“每帧调用函数”描述 UI，让状态显式留在应用侧。
- 即时模式不是低级，也不是玩具，它适合工具、编辑器、调试器。

对应后文：

- ImGui 对照。
- 保留模式 vs 即时模式。
- 渲染管线和帧调度。

### 7. 声明式 / 响应式传统

关键词：data -> UI、binding、signals、FRP、Elm Architecture、SwiftUI、Flutter、Slint。

可写内容：

- 声明式 UI 的问题不是“怎么画按钮”，而是“状态变化后谁负责同步界面”。
- Binding、Observable、Signal、Reducer 是同一族问题的不同答案。
- 声明式框架把“重建 UI 描述”变便宜，再用 diff / retained engine / scene graph 落地。

对应后文：

- Anatomy GUI v2 的 Property / Binding。
- WinUI3 DependencyProperty。
- Slint / Flutter / React 作为旁证。

---

## 三、核心概念词典

建议写成 12 个理论章，每章后面都能回链到 Win32、Anatomy GUI v2 和工业框架。

| # | 理论章 | 核心问题 | 后续落点 |
|:---:|:---|:---|:---|
| 1 | GUI 的 12 个器官 | 一个 GUI 框架到底由什么组成 | 全书索引 |
| 2 | 事件循环与主线程 | 为什么 GUI 是事件驱动，为什么 UI 线程特殊 | Win32 message loop、glib、DispatcherQueue |
| 3 | 对象身份与生命周期 | 控件为什么需要身份，谁拥有谁 | HWND、GObject、QObject、DependencyObject |
| 4 | 控件树、视觉树与语义树 | 树不止一棵，各自服务什么 | Layout、render、a11y |
| 5 | 布局理论 | 约束、盒模型、measure/arrange、绝对定位 | Win32 MoveWindow、WinUI Panel、CSS |
| 6 | 绘制、脏区与合成 | 谁决定何时重画，谁把像素合成到屏幕 | GDI、D2D、DWM、GPU compositor |
| 7 | 输入、焦点与命令路由 | 鼠标键盘之后还有捕获、焦点、快捷键、IME | WndProc、responder chain、command pattern |
| 8 | 文本与国际化 | 文本不是字符串，输入也不是按键 | Unicode、IME、shaping、BiDi、font fallback |
| 9 | 样式、主题与设计系统 | 样式如何从代码里独立出来 | CSS、Qt style、WinUI ResourceDictionary |
| 10 | 状态、绑定与响应式 | 数据变化如何驱动界面变化 | Signal、Observable、MVVM、Elm |
| 11 | 异步与并发 | 为什么不能阻塞 UI，后台任务如何回投 | coroutine、Dispatcher、PostMessage |
| 12 | 可访问性与语义 | 屏幕上有像素不等于可被理解 | UIA、ARIA、semantic tree |

---

## 四、建议章节草案

### T00 · 为什么 GUI 不是 API 清单

- GUI 框架的共同问题。
- “能写窗口”与“理解框架”的差别。
- 课程阅读方式：理论 -> 标本 -> 手搓 -> 比较 -> 产品化。

### T01 · GUI 的 12 个器官

- 窗口与显示面。
- 事件循环。
- 输入系统。
- 对象模型。
- 控件树。
- 布局。
- 绘制与合成。
- 文本系统。
- 样式系统。
- 状态绑定。
- 异步并发。
- 可访问性与交付。

### T02 · 保留模式 vs 即时模式

- 保留模式：框架保存控件树和状态。
- 即时模式：应用每帧描述 UI。
- 二者不是新旧关系，而是状态所有权的不同。
- 用一个计数器、一个表单、一个列表展示差异。

### T03 · 命令式、声明式与响应式

- 命令式：调用 API 改 UI。
- 声明式：描述状态对应的 UI。
- 响应式：状态变化自动传播。
- 为什么声明式仍需要底层保留树或渲染引擎。

### T04 · MVC / MVP / MVVM / Elm Architecture

- 这些模式解决的都是状态、视图、输入之间的循环。
- 桌面 GUI 中 MVVM 为什么常见。
- Elm/Redux 为什么适合讲清“单向数据流”。

### T05 · 事件循环是 GUI 的心脏

- 阻塞等待、消息分发、重入。
- UI 线程亲和。
- 嵌套消息循环、模态对话框。
- 为什么协程要知道 dispatcher。

### T06 · 控件不是矩形，而是对象

- 身份、父子关系、生命周期。
- native widget vs lightweight widget。
- handle、QObject、GObject、DependencyObject、React component 的差异。

### T07 · 输入法、焦点与文本是 GUI 的深水区

- Key down 不等于文字输入。
- IME composition。
- 光标、选择、编辑命令。
- 中文读者必须看到这一章。

### T08 · 可访问性不是附加功能

- 语义树与视觉树分离。
- 屏幕阅读器需要 role/name/value/state。
- 自绘控件为什么容易坏。

### T09 · GUI 的隐藏成本

- DPI、国际化、主题、高对比度。
- 打包、更新、崩溃、测试。
- 真实软件和 demo 的差别。

---

## 五、配套小实验

- 消息循环模拟器：不用 Win32，手写一个 queue + dispatch。
- 保留/即时计数器：同一交互写成两种范式。
- 控件树可视化：打印 parent、children、bounds、focus。
- 布局求解器玩具：absolute / box / measure-arrange 三种。
- 脏区可视化：把 invalid region 画出来。
- IME 事件日志器：记录 key、composition、commit 的差异。
- 可访问性树草图：同一个按钮的视觉、布局、语义三棵树。

---

## 六、这一板块的边界

该板块不应该变成纯历史综述。每个理论都必须服务后文：

- 讲传统，是为了理解今天的 API 为什么有这些形状。
- 讲模式，是为了能比较框架，而不是背名词。
- 讲思想，是为了给 Anatomy GUI v2 的实现做铺垫。
