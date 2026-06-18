# 自己动手写 GUI · MiniUI Handbook

> 用 XCB + Cairo，在 Linux 上从零手搓一个**教学级 GUI 框架**。
> 不复用任何现成框架，把一个 GUI 该有的器官——窗口、事件循环、控件树、布局、信号、渲染管线、并发——亲手造一遍。

## 为什么有这个板块

前面的 Win32 篇是「解剖标本」：看一个已经造好、高度透明的框架长什么样。本篇反过来——**自己当一次框架作者**。当你在 XCB 裸接口上把事件循环、控件树、布局引擎、脏区重绘一个一个搭起来之后，再看 GTK、Qt、WinUI 这些框架，都只是你已熟知的器官的变奏。

## 前置知识

- **C++ 基础**：RAII、智能指针、模板、lambda 与回调
- **Linux 开发**：能用命令行、装得了 `libxcb1-dev libcairo2-dev`
- **建议先读**：Win32 篇前几章，建立「窗口 + 消息循环」的心智模型；不读也能跟，但有了对照会更快

## 开发环境

```bash
# Debian / Ubuntu / WSL2
sudo apt-get install -y libxcb1-dev libcairo2-dev
```

WSL2 下可用 XLaunch 或 WSLg 显示窗口。

## 章节列表

本系列共 10 个 Stage，每个 Stage 引入一个 GUI 框架的核心器官，在前一阶段的基础上递进生长：

| Stage | 章节 | 文件 |
|:-----:|:-----|:-----|
| 1 | 在 X11 世界里打开一扇窗 | [62_xcb_window](handbook/62_xcb_window.md) |
| 2 | 事件循环：GUI 的心跳 | [63_event_loop](handbook/63_event_loop.md) |
| 3 | 让窗口学会画画：Cairo 绘制原语 | [64_cairo_drawing](handbook/64_cairo_drawing.md) |
| 4 | 从白板到控件树：GUI 框架的灵魂 | [65_widget_tree](handbook/65_widget_tree.md) |
| 5 | 布局引擎：让控件自己找到位置 | [66_layout_engine](handbook/66_layout_engine.md) |
| 6 | 信号系统与响应式属性：让控件学会对话 | [67_signals](handbook/67_signals.md) |
| 7 | 渲染管线与异步 IO：双缓冲、脏区管理与不冻结的 GUI | [68_render_pipeline](handbook/68_render_pipeline.md) |
| 8 | Mini 文本编辑器：用协程缝合一切 | [69_mini_editor](handbook/69_mini_editor.md) |
| 9 | 响应式事件流：Observable 与 Rx 模式 | [70_observable](handbook/70_observable.md) |
| 10 | 线程池与并发 GUI：让界面不再卡死 | [71_threadpool](handbook/71_threadpool.md) |

## 配套代码

每个 Stage 对应 [`src/tutorial/anatomy_gui_for_tutorials/`](https://github.com/Charliechen114514/anatomy_gui/tree/main/src/tutorial/anatomy_gui_for_tutorials) 下的 `stage1` ~ `stage10`，可独立编译：

- **Stage 1–3**：窗口、事件循环、Cairo 绘制——搭起一块能动的画布
- **Stage 4–6**：控件树、布局引擎、信号系统——长出框架的骨架
- **Stage 7–10**：渲染管线、协程、响应式、线程池——把骨架变成不卡顿的产品

```bash
cmake -B build -S src/tutorial/anatomy_gui_for_tutorials/stage1
cmake --build build --config Release
```
