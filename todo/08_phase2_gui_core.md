# Phase 2 — GUI 核心概念（第二篇）

> 跨框架通用知识，为后续各框架教程建立理论框架

## 前置依赖

- Phase 0（Qt 内容移除）
- Phase 1（Win32 进阶，提供完整的 Win32 示例用于对比）

## 教程文件（`tutorial/gui_core/`）

> 新建 `tutorial/gui_core/` 目录，添加 `.pages` 文件。

| 编号 | 文件名 | 标题 | Roadmap 参考 |
|:-----|:-------|:-----|:-------------|
| 62 | `62_ProgrammingGUI_Core_EventDriven.md` | 事件驱动编程模型 | 2.1 (L294-309) |
| 63 | `63_ProgrammingGUI_Core_Layout.md` | 控件树与布局系统 | 2.2 (L313-329) |
| 64 | `64_ProgrammingGUI_Core_MVx.md` | MVC / MVP / MVVM 模式 | 2.3 (L333-349) |
| 65 | `65_ProgrammingGUI_Core_RenderPipeline.md` | 绘制流水线 | 2.4 (L353-368) |
| 66 | `66_ProgrammingGUI_Core_FontText.md` | 字体与文字渲染 | 2.5 (L372-388) |
| 67 | `67_ProgrammingGUI_Core_ResourceMgmt.md` | 资源管理模式 | 2.6 (L392-416) |

### 各节内容要求

每节统一结构：概念讲解 → 代码结构（多框架对比） → 常见坑 → 小作业

- **62 事件驱动**：Win32 消息循环 / GTK `gtk_main()` / WinUI3 `Dispatcher` 三者对比，抽象共同骨架
- **63 布局系统**：同一"登录表单"用 Win32 绝对坐标 / GTK Grid / WinUI3 XAML 三种实现并排对比
- **64 MVx 模式**：Win32 手动 MVP（IView 接口） / WinUI3 MVVM（{x:Bind}）对比
- **65 绘制流水线**：CPU(GDI) vs GPU(Direct2D) 对比，脏区机制详解
- **66 字体文字**：GDI GetTextMetrics / DirectWrite / Pango 字体度量对比
- **67 资源管理**：RAII 封装 GDI 对象、跨平台资源抽象策略

## 代码示例（`src/tutorial/gui_core/`）

| 目录 | 内容 |
|:-----|:-----|
| `01_event_loop/` | Win32/GTK/WinUI3 事件循环最小对比示例 |
| `02_layout_compare/` | 同一表单，三种布局实现 |
| `03_mvp_win32/` | Win32 MVP 模式完整示例 |
| `04_dirty_rect/` | 脏区最小化绘制示例 |
| `05_font_metrics/` | GDI/DirectWrite 字体度量对比 |
| `06_raii_gdi/` | RAII 资源管理封装（ScopedSelectObject 等） |

## 完成后更新

- [ ] `tutorial/index.md` — 添加第二篇表格
- [ ] `Roadmap.md` — 更新第二篇完成度 0% → 100%
- [ ] `CHANGELOG.md` — 记录变更
