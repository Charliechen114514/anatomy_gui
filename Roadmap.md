# GUI 编程系列 · 完整路线图

> **定位**：面向掌握 C++ 基础语法的读者，循序渐进，概念先行，覆盖 Win32 / Qt / WinUI / GTK / wxWidgets / Web 前端，配套 GitHub 代码仓库与博客文章。

---

## 总体结构概览

```
第〇篇  前言与准备          ← 世界观建立
第一篇  Win32 原生编程      ← 地基（进行中 75%）
第二篇  GUI 核心概念        ← 跨框架通识
第三篇  图形渲染            ← GDI → Direct2D → GPU
第四篇  跨平台框架          ← Qt / wxWidgets / GTK
第五篇  现代 Windows 技术   ← WinUI 3 / WebView2
第六篇  Web 混合方案        ← Electron / CEF / Tauri
第七篇  工程化专题          ← 多线程、DPI、打包、i18n
第八篇  综合实战项目        ← 贯穿全系列的收尾
```

---

## 短期规划

> 当前聚焦：完成第一篇 Win32 原生编程，夯实基础后再进入后续内容。

### 第一篇 · Win32 原生编程（进行中）

**核心目标**：彻底理解 Windows GUI 的底层运作机制，这是读懂一切上层框架的基础。

| 章节 | 内容 | 状态 | 对应文件 |
|:-----|:-----|:-----|:--------|
| 1.1 | Windows 程序的本质 | ✅ 已完成 | `1_ProgrammingGUI_NativeWindows.md` |
| 1.2 | 消息机制 | ✅ 已完成 | `2_ProgrammingGUI_NativeWindows_2.md` |
| 1.3 | 常用系统消息 | ✅ 已完成 | `5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md` |
| 1.3补充 | DPI 适配专题 | ✅ 已完成 | `3_ProgrammingGUI_WhatAboutDPI.md` |
| 1.4 | 标准控件 | ✅ 已完成 | `4_`, `6_`, `7_`, `8_` (控件系列) |
| 1.5 | 窗口布局与资源 | ✅ 已完成 | `9_` ~ `17_` (对话框与资源系列) |
| 1.6 | GDI 基础绘图 | ❌ 待完成 | — |
| 1.7 | Win32 进阶 | ❌ 待完成 | — |
| 阶段小项目 | 纯 Win32 文本编辑器 | ❌ 待完成 | — |

#### 1.4 标准控件（已完成）

**基础控件：**
- [x] 按钮、编辑框、静态控件 → `4_ProgrammingGUI_NativeWindows_Controls.md`
- [x] WM_NOTIFY 机制 → `5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md`

**高级控件：**
- [x] ListView 控件 → `6_ProgrammingGUI_NativeWindows_ListView.md`
- [x] TreeView 控件 → `7_ProgrammingGUI_NativeWindows_TreeView.md`
- [x] 更多控件（标签页、进度条、滚动条等）→ `8_ProgrammingGUI_NativeWindows_MoreControls.md`

#### 1.5 窗口布局与资源（已完成）

**对话框：**
- [x] 模态对话框（DialogBox、DialogBoxParam、WM_INITDIALOG）→ `9_ProgrammingGUI_NativeWindows_ModalDialog.md`
- [x] 非模态对话框（CreateDialog、消息循环集成）→ `10_ProgrammingGUI_NativeWindows_ModelessDialog.md`
- [x] 对话框过程 vs 窗口过程 → `11_ProgrammingGUI_NativeWindows_DialogProc.md`

**资源文件：**
- [x] .rc 文件结构、resource.h、rc.exe 编译 → `12_ProgrammingGUI_NativeWindows_ResourceFiles.md`
- [x] 菜单资源（MENU、弹出菜单、动态创建）→ `13_ProgrammingGUI_NativeWindows_MenuResource.md`
- [x] 图标、光标、位图资源 → `14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md`
- [x] 字符串表（String Table、国际化）→ `15_ProgrammingGUI_NativeWindows_StringTable.md`
- [x] 对话框模板（DIALOG、DIALOGEX）→ `16_ProgrammingGUI_NativeWindows_DialogTemplate.md`

**资源编辑器：**
- [x] Visual Studio Resource Editor 使用 → `17_ProgrammingGUI_NativeWindows_VSResourceEditor.md`

#### 1.6 GDI 基础绘图

**设备上下文（HDC）：**
- [ ] BeginPaint/EndPaint、GetDC/ReleaseDC、GetWindowDC、CreateCompatibleDC
- [ ] 设备上下文状态（SaveDC、RestoreDC）
- [ ] GDI 对象生命周期（DeleteObject、SelectObject）

**GDI 绘图对象：**
- [ ] 画笔（HPEN）、画刷（HBRUSH）、字体（HFONT）、区域（HRGN）

**基本图形绘制：**
- [ ] 线条（MoveToEx、LineTo、Polyline、PolyBezier）
- [ ] 矩形与多边形（Rectangle、Ellipse、Polygon）
- [ ] 文字输出（TextOut、DrawText、GetTextExtentPoint32）
- [ ] 位图操作（BitBlt、StretchBlt、TransparentBlt、AlphaBlend）

**双缓冲技术：**
- [ ] 消除闪烁原理、WM_ERASEBKGND 处理
- [ ] 完整双缓冲实现

#### 1.7 Win32 进阶

- [ ] 子类化（Subclassing）与超类化（Superclassing）
- [ ] Hook 机制（WH_KEYBOARD_LL、WH_MOUSE_LL 等）
- [ ] 系统托盘（Shell_NotifyIcon、NOTIFYICONDATA）
- [ ] 拖放（Drag & Drop）（IDropTarget、WM_DROPFILES）
- [ ] 定时器（SetTimer / WM_TIMER）

#### 阶段小项目：纯 Win32 文本编辑器

**功能规格：**
- [ ] 菜单栏（文件、编辑、帮助菜单）
- [ ] 工具栏（新建、打开、保存、剪切、复制、粘贴）
- [ ] 状态栏（行号、列号、文件名、修改状态）
- [ ] 核心功能（Edit/RichEdit 控件、文件对话框、快捷键）
- [ ] 可选扩展（查找/替换、行号显示、语法高亮）

#### 需补充的现有章节

**1.2 消息机制补充：**
- [ ] SendMessage vs PostMessage 详细对比
- [ ] 消息队列详解（发送队列 vs 投递队列）
- [ ] 其他消息函数（SendNotifyMessage、PostThreadMessage 等）

**1.3 常用系统消息补充：**
- [ ] 更多输入消息（WM_MOUSEWHEEL、WM_TOUCH、WM_INPUT）
- [ ] 更多窗口消息（WM_GETMINMAXINFO、WM_NCHITTEST 等）
- [ ] 更多绘制消息（WM_ERASEBKGND、WM_PRINTCLIENT）

---

## 长期规划

> 第一篇完成后，根据反馈和时间安排推进后续内容。

## 第〇篇 · 前言与准备

### 0.1 为什么要学 GUI 编程？
- GUI 程序的组成要素：窗口、消息、渲染、控件
- 与 Web 前端、移动端的横向对比
- C++ GUI 的生态现状

### 0.2 开发环境搭建
- Windows：Visual Studio 2022 + MSYS2/MinGW
- Linux/macOS 补充说明
- 推荐工具：Spy++、WinSpy、资源监视器

### 0.3 本系列的学习方法
- 如何阅读官方文档
- 配套 GitHub 仓库结构说明

---

## 第二篇 · GUI 核心概念（跨框架通识）

**核心目标**：抽象出所有 GUI 框架共有的设计思想，建立框架无关的思维模型。

### 2.1 事件驱动编程模型
- 事件循环的通用模型
- 事件 vs 消息 vs 信号
- 同步事件与异步事件

### 2.2 控件树与布局系统
- 父子控件树的普遍性
- 绝对布局 vs 流式布局 vs 弹性布局
- 各框架布局实现对比

### 2.3 MVC / MVP / MVVM 模式
- 为什么 GUI 需要架构模式
- Qt Model/View 框架
- Win32/GTK 中手动实现 MVP

### 2.4 绘制流水线
- CPU 绘制 vs GPU 绘制
- 立即模式 vs 保留模式
- 脏区与重绘优化

### 2.5 字体与文字渲染
- 字体度量（Metrics）
- 各框架文字渲染引擎对比
- Unicode 与多语言支持

### 2.6 资源管理模式
- 图片、字体、字符串的统一管理
- 平台资源 vs 跨平台资源

---

## 第三篇 · 图形渲染专题

### 3.1 GDI 深入
- GDI 对象生命周期
- 区域与裁切
- Alpha 混合

### 3.2 GDI+ 入门
- GDI+ vs GDI
- 抗锯齿与高质量渲染
- 图像格式支持

### 3.3 Direct2D + DirectWrite
- 为什么需要 Direct2D
- Direct2D 资源与 HWND RenderTarget
- DirectWrite 高质量文字渲染

### 3.4 自定义控件绘制原理
- Owner-Draw 控件
- 完全自绘控件架构
- 命中测试实现

### 3.5 OpenGL / Vulkan 与 GUI 集成
- 在 Win32/Qt 窗口中嵌入 OpenGL
- Qt QOpenGLWidget 封装

---

## 第四篇 · 跨平台框架

### ── Qt 篇 ──

### 4.1 Qt 的世界观
- Qt 历史与生态（Qt5 vs Qt6）
- qmake/CMake 构建系统
- QObject、对象树、MOC

### 4.2 Qt 信号与槽
- 信号与槽声明与连接
- 连接类型：直接 vs 队列
- Lambda 作为槽函数

### 4.3 Qt Widgets 布局系统
- QHBoxLayout/VBoxLayout/GridLayout/FormLayout
- 伸缩因子与 SizePolicy

### 4.4 Qt 常用控件全览
- 基础控件、容器控件、视图控件

### 4.5 Qt Model/View 架构
- QAbstractItemModel 实现
- 标准模型与自定义 Delegate

### 4.6 Qt Style Sheets（QSS）
- QSS 与 CSS 异同
- 常见美化示例

### 4.7 Qt 多线程
- QThread 正确用法
- Worker Object 模式

### 4.8 Qt 网络与文件
- QFile/QDir/QFileSystemWatcher
- QTcpSocket/QHttpClient

> **阶段小项目**：用 Qt 重新实现「文本编辑器」

### ── wxWidgets 篇 ──

### 4.9 wxWidgets 设计哲学
- 原生控件策略
- 事件表机制

### 4.10 wxWidgets 核心控件与布局
- Sizer 系统
- XRC 资源文件

> **阶段小项目**：wxWidgets 跨平台「图片查看器」

### ── GTK 篇 ──

### 4.11 GTK 生态与 C++ 绑定
- GTK4 vs GTK3
- Meson + pkg-config

### 4.12 GTK 信号机制与 GObject
- GObject 类型系统
- gtkmm C++ 封装

### 4.13 GTK 布局容器
- GtkBox/GtkGrid/GtkOverlay
- CSS 主题系统

> **阶段小项目**：GTK/gtkmm「系统信息面板」

---

## 第五篇 · 现代 Windows 技术

### 5.1 WinUI 3 与 Windows App SDK
- UWP → WinUI 3 演进
- XAML 标记语言基础
- Fluent Design

### 5.2 XAML 数据绑定与 MVVM
- {Binding} vs {x:Bind}
- ObservableCollection 与 ICommand

### 5.3 WinRT API 的使用
- C++/WinRT 语法
- 文件选择器、通知、剪贴板

### 5.4 打包为 MSIX
- MSIX 包结构
- 应用商店发布流程

---

## 第六篇 · Web 混合方案

### 6.1 为什么选择混合方案？
- 原生 UI vs Web UI 权衡
- 技术路线对比

### 6.2 WebView2
- WebView2 架构
- JavaScript ↔ C++ 双向通信

### 6.3 CEF
- CEF 与 WebView2 区别
- CEF 进程模型

### 6.4 Electron 简介
- Electron 进程模型
- 与 C++ 原生模块交互

---

## 第七篇 · 工程化专题

### 7.1 多线程与 UI 线程安全
- GUI 黄金法则
- 各框架线程调度方法对比

### 7.2 DPI 适配与高分辨率支持
- DPI 基本概念
- 各框架 DPI 处理

### 7.3 自定义控件设计模式
- 控件通用接口设计
- 状态机与动画系统

### 7.4 打包与部署
- Windows：静态/动态链接、Inno Setup/NSIS、MSIX
- Linux：AppImage/Flatpak/Snap
- macOS：.app Bundle、DMG

### 7.5 国际化与本地化
- 字符编码转换
- 各框架翻译机制

### 7.6 辅助功能（Accessibility）
- Windows UI Automation
- ARIA 与 Qt Accessibility

### 7.7 性能优化
- 重绘性能优化
- 虚拟列表实现
- 启动速度优化

---

## 第八篇 · 综合实战项目

### 项目：跨平台 Markdown 编辑器

**功能规格：**
- 左侧编辑区 + 右侧实时预览
- 文件打开、保存、最近文件
- 工具栏 + 菜单栏 + 状态栏
- 语法高亮、系统主题跟随

**各框架实现版本：**

| 框架 | 侧重展示的知识点 |
|------|----------------|
| Win32 + WebView2 | 第一、六篇的综合应用 |
| Qt | Model/View、QSS 主题、多线程 |
| wxWidgets | 跨平台构建、原生外观 |
| GTK/gtkmm | Linux 原生集成 |
| WinUI 3 | XAML、Fluent Design、MSIX |

---

> 一直在路上，持续更新中...
