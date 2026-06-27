# 草稿 04 · 工业框架比较解剖扩展

> 目标：保留原 Act 2 “按器官比较”的独特性，但扩大样本和器官范围。主样本保持可控，旁证样本用于打开视野，不把全书拖成“所有框架用法大全”。

---

## 一、样本分级

### A 级主样本：正文深解

这些框架值得放进每个器官章正文：

| 样本 | 角色 |
|:---|:---|
| Win32 | 最透明的传统底层标本 |
| Anatomy GUI v2 | 自己造的骨架，用来验证理论 |
| GTK4 | Linux 工业 Toolkit，GObject / CSS / Snapshot |
| Qt 6 | C++ GUI 工业巨头，QObject / signals / QML / Widgets |
| WinUI 3 | 现代 Windows 官方 UI 栈，XAML / WinRT / Composition |
| Dear ImGui | 即时模式代表，工具型 UI 标本 |

### B 级旁证样本：短框或专题

这些不一定每章都写完整，但可作为“旁证”：

| 样本 | 用在 |
|:---|:---|
| Web DOM / React | 声明式、DOM、CSS、事件冒泡、虚拟 DOM |
| Slint | C++ 友好的声明式 UI，适合响应式和样式对照 |
| Flutter | retained engine + declarative rebuild + Skia/Impeller |
| WPF | 经典 XAML / DependencyProperty / MVVM 对照 |
| Cocoa / SwiftUI | responder chain、run loop、声明式 UI 旁证 |
| Avalonia | 跨平台 XAML，与 WPF/WinUI 的思想继承 |
| Electron / Tauri | Web 技术桌面化，放在混合应用和交付章 |

### C 级参考样本：附录

- FLTK。
- wxWidgets。
- Java Swing / JavaFX。
- Unreal Slate。
- Unity UI Toolkit。
- EFL。

用途：证明某个器官还有其他变体，不进入主线。

---

## 二、器官范围扩展

原规划 8 个器官很好，但可以扩展为 12 个：

| # | 器官 | 核心问题 |
|:---:|:---|:---|
| 1 | 应用模型、窗口与显示面 | 程序如何成为桌面应用，窗口如何连接显示系统 |
| 2 | 事件循环与调度器 | 谁等待事件，谁分发事件，谁决定 UI 线程 |
| 3 | 输入、焦点与命令路由 | 鼠标、键盘、触控、快捷键、菜单命令如何流动 |
| 4 | 对象模型与生命周期 | 控件身份、类型系统、所有权、销毁 |
| 5 | 控件树、视觉树、语义树 | UI 内部到底维护了哪些树 |
| 6 | 布局系统 | 尺寸如何协商，约束如何求解 |
| 7 | 绘制、脏区与合成 | 像素何时生成，谁缓存，谁合成 |
| 8 | 文本、字体与输入法 | 文本排版、字体 fallback、IME、选择、编辑 |
| 9 | 样式、主题与资源 | 颜色、字体、状态、主题、资源如何组织 |
| 10 | 状态、信号与绑定 | 数据变化如何驱动 UI |
| 11 | 异步、线程与任务 | 后台工作如何不冻结 UI |
| 12 | 可访问性、测试与交付 | GUI 如何被读屏、测试、打包和维护 |

---

## 三、每个器官章的统一结构

建议所有比较章使用同一种格式：

1. **器官定义**：它解决什么问题，不解决什么问题。
2. **Anatomy GUI v2 回顾**：我们自己怎么实现它。
3. **Win32 标本**：最低层长什么样。
4. **GTK4 标本**：GObject / GTK 传统怎么表达。
5. **Qt 6 标本**：QObject / Widgets / QML 的答案。
6. **WinUI3 标本**：XAML / DependencyObject / Composition 的答案。
7. **ImGui 反例或对照**：即时模式如何绕开或改写这个器官。
8. **旁证短框**：Web / Slint / Flutter 等。
9. **横向表格**：身份、所有权、线程、绘制、绑定、a11y 等维度。
10. **同题 Demo**：同一交互在 2-4 个样本中实现。
11. **设计点评**：这些选择背后的历史原因和代价。

---

## 四、12 个器官章草案

### C01 · 应用模型、窗口与显示面

问题：

- 程序如何接入操作系统桌面？
- 窗口是 OS 对象、框架对象，还是渲染 surface？
- 多窗口、子窗口、popup、tooltip 如何定位？

对照：

- Win32：`HWND`。
- GTK：`GtkApplication`、`GtkWindow`、surface。
- Qt：`QApplication`、`QWidget`、`QWindow`。
- WinUI3：AppWindow、Window、XAML content。
- ImGui：viewport、platform backend。
- Web：browser tab / window / canvas。

Demo：

- 最小窗口 + resize 日志 + DPI 日志。

### C02 · 事件循环与调度器

问题：

- 谁拥有主循环？
- 事件分发和异步任务如何排队？
- 嵌套循环和模态对话框为什么危险？

对照：

- `GetMessage`。
- glib main loop。
- Qt event loop。
- DispatcherQueue。
- ImGui frame loop。
- JavaScript event loop。

Demo：

- 一个 timer + async task + UI update 的时间线可视化。

### C03 · 输入、焦点与命令路由

问题：

- 鼠标事件如何命中控件？
- 焦点如何移动？
- 快捷键、菜单、命令如何找到接收者？
- 触控和 Pointer 如何进入传统鼠标模型？

对照：

- Win32 hit test / focus / accelerator。
- GTK event controllers。
- Qt event filter / shortcut。
- WinUI routed event / command。
- Cocoa responder chain 作为旁证。

Demo：

- 一个含菜单、快捷键、文本框、按钮的焦点路线图。

### C04 · 对象模型与生命周期

问题：

- 控件是什么类型的对象？
- 谁持有它？
- 销毁时事件和异步任务怎么办？

对照：

- Win32 handle + user data。
- GObject ref-count。
- QObject parent ownership。
- WinRT / COM ref-count。
- ImGui 无持久控件对象。

Demo：

- 动态创建和销毁列表项，记录构造/析构/引用变化。

### C05 · 控件树、视觉树、语义树

问题：

- parent-child 关系服务布局、渲染还是可访问性？
- native child window 与 lightweight widget 的区别？
- 即时模式有没有“树”？

对照：

- HWND tree。
- GTK widget tree。
- Qt object tree / item tree。
- WinUI logical / visual tree。
- Web DOM / accessibility tree。

Demo：

- 控件树 dump + bounds overlay + semantic dump。

### C06 · 布局系统

问题：

- 子控件如何表达想要多大？
- 父控件如何分配空间？
- 绝对定位、盒模型、约束布局各自代价是什么？

对照：

- Win32 `MoveWindow`。
- GTK measure / allocate。
- Qt layout。
- WinUI measure / arrange。
- CSS flex / grid。
- ImGui manual layout。

Demo：

- 同一设置表单，窗口缩放时对照布局策略。

### C07 · 绘制、脏区与合成

问题：

- 谁决定何时重绘？
- 脏区如何合并？
- CPU raster、GPU compositor、scene graph 是什么关系？

对照：

- Win32 `WM_PAINT` / GDI / D2D。
- GTK snapshot。
- Qt paint event / scene graph。
- WinUI composition。
- ImGui draw list。
- Flutter / Skia 旁证。

Demo：

- 脏区可视化 + FPS / paint count。

### C08 · 文本、字体与输入法

问题：

- 字符串如何变成 glyph？
- IME composition 如何进入文本框？
- 字体 fallback、BiDi、emoji、selection 怎么处理？

对照：

- Win32 IME + DirectWrite。
- GTK Pango。
- Qt QTextLayout。
- WinUI TextBox / DirectWrite。
- Web text engine。

Demo：

- 中文输入、emoji、混合英文、选区、caret 的文本实验。

### C09 · 样式、主题与资源

问题：

- 样式是代码、资源，还是语言？
- 状态样式如何表达：hover、pressed、disabled、focused？
- 深色模式和高对比度如何进入框架？

对照：

- Win32 theme API / DWM。
- GTK CSS。
- Qt stylesheet / palette。
- WinUI ResourceDictionary。
- Web CSS。
- Slint theme。

Demo：

- 同一个按钮的状态样式矩阵。

### C10 · 状态、信号与绑定

问题：

- 控件变化如何通知外界？
- 数据变化如何回到 UI？
- 双向绑定有什么风险？

对照：

- Win32 `WM_COMMAND`。
- GObject signal。
- Qt signal/slot。
- WinUI DependencyProperty / Binding。
- React state。
- ImGui return value。

Demo：

- 同一个计数器、表单校验、列表过滤。

### C11 · 异步、线程与任务

问题：

- UI 线程为什么不能阻塞？
- 后台任务如何回到 UI？
- 取消、进度、错误如何表达？

对照：

- Win32 `PostMessage`。
- glib idle / async。
- Qt signal across threads。
- WinUI `IAsyncOperation` / dispatcher。
- JavaScript promise。

Demo：

- 异步加载 10MB 文件，带取消和进度。

### C12 · 可访问性、测试与交付

问题：

- 自绘控件如何被屏幕阅读器理解？
- GUI 如何做自动化测试？
- 工业框架如何处理打包和更新？

对照：

- Win32 UIA。
- GTK AT-SPI。
- Qt accessibility。
- WinUI AutomationPeer。
- Web ARIA。

Demo：

- 一个自定义控件的 a11y provider 对照。

---

## 五、框架实操章

比较解剖之外，还需要少量“怎么上手”的 specimen 章：

- GTK4 / gtkmm：工程结构、signals、CSS、Inspector。
- Qt 6 Widgets：QObject、signals、layouts、资源、Designer 可选。
- Qt 6 QML：声明式、property binding、C++ 互操作。
- WinUI3：Windows App SDK、XAML、C++/WinRT、MSIX。
- ImGui：backend、frame loop、dock、tools UI。
- WebView2：原生壳与 Web UI。
- Slint：声明式 C++ GUI 旁证。

这些章只讲 idiom 和工具链，不重复器官原理。

---

## 六、避免失控的规则

- 主线不追求“教会所有框架”。
- 每个器官只深挖 4-6 个样本。
- 旁证样本只在它能照亮一个设计差异时出现。
- 每个比较章必须回到 Anatomy GUI v2，否则会变成资料拼贴。
- 每章至少有一个表格和一个同题 demo。
