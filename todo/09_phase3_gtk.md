# Phase 3 — 跨平台框架 GTK（第四篇）

> 移除 Qt 和 wxWidgets 后，第四篇仅保留 GTK，并扩展为完整覆盖

## 前置依赖

- Phase 2（GUI 核心概念，提供跨框架理论框架）

## 教程文件（`tutorial/gtk/`）

> 新建 `tutorial/gtk/` 目录，添加 `.pages` 文件。

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 68 | `68_ProgrammingGUI_GTK_Intro.md` | GTK 生态与 C/C++ 绑定 | GTK3/4 历史、构建系统（Meson/vcpkg）、gtkmm 介绍 |
| 69 | `69_ProgrammingGUI_GTK3_Basics.md` | GTK3 基础：控件/布局/事件 | GtkApplication、GtkWindow、GtkBox、GtkGrid、事件处理 |
| 70 | `70_ProgrammingGUI_GTK4_Basics.md` | GTK4 基础：Snapshot 模型/新 API | GtkApplication、Snapshot 渲染、LayoutManager、新事件模型 |
| 71 | `71_ProgrammingGUI_GTK_Signal.md` | 信号机制与 GObject | GObject 类型系统、g_signal_connect、gtkmm signal_xxx、属性通知 |
| 72 | `72_ProgrammingGUI_GTK_Layout.md` | 布局容器与 CSS 主题 | GtkBox/Grid/Overlay/Paned、GtkCssProvider、GTK Inspector |
| 73 | `73_ProgrammingGUI_GTK_CustomWidget.md` | GTK 自定义控件 | 绘制、状态机、无障碍（gtkmm）、事件路由 |
| 74 | `74_ProgrammingGUI_GTK_gtkmm.md` | gtkmm C++ 绑定深度使用 | RAII、Lambda 信号、自定义 Widget、CMake 集成 |
| 75 | `75_ProgrammingGUI_GTK_StageProject.md` | 阶段项目：系统信息面板 | Linux 原生风格，显示 CPU/内存/磁盘信息 |

### 各节内容要求

- GTK3 和 GTK4 **并重**，每节都覆盖两个版本的差异
- C API (GTK) 和 C++ API (gtkmm) 都要展示
- 构建：Linux 用 Meson/pkg-config，Windows 用 vcpkg 或 MSYS2
- 每节统一结构：概念讲解 → 代码结构 → 常见坑 → 小作业

## 代码示例（`src/tutorial/gtk/`）

> Linux 原生构建（Meson 或 CMake），CI 需要 ubuntu-latest 工作流

| 目录 | 内容 |
|:-----|:-----|
| `01_hello_gtk3/` | GTK3 最小窗口 |
| `02_hello_gtk4/` | GTK4 最小窗口 |
| `03_signals/` | 信号连接三种方式对比 |
| `04_layout_grid_box/` | Grid/Box 布局示例 |
| `05_css_theming/` | CSS 主题定制 |
| `06_custom_widget/` | 自定义绘制控件 |
| `07_gtkmm_basics/` | gtkmm C++ 基础 |
| `exercises/01_sysinfo_panel/` | 系统信息面板阶段项目 |

## 完成后更新

- [ ] `tutorial/index.md` — 添加第四篇 GTK 表格
- [ ] `Roadmap.md` — 移除 Qt/wxWidgets 章节，重新编号第四篇，更新进度
- [ ] `README.md` — 更新技术徽章（保留 GTK，移除 Qt/wxWidgets）
- [ ] `CHANGELOG.md` — 记录变更
