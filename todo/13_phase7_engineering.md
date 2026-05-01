# Phase 7 — 工程化专题（第七篇）

> 覆盖实际项目开发中的关键工程化主题

## 前置依赖

- Phase 1-3（提供多框架背景用于跨框架对比）

## 教程文件

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 92 | `92_ProgrammingGUI_Engineering_ThreadSafety.md` | 多线程与 UI 线程安全 | PostMessage/QMetaObject/Dispatcher/g_idle_add、线程安全队列、Worker 模式 |
| 93 | `93_ProgrammingGUI_Engineering_DPI.md` | DPI 适配与高分辨率支持 | Per-Monitor V2、各框架 DPI 处理、双显示器测试、DPI 感知声明 |
| 94 | `94_ProgrammingGUI_Engineering_Packaging.md` | 打包与部署 | Inno Setup / MSIX / AppImage / DMG、VC Redist、代码签名 |
| 95 | `95_ProgrammingGUI_Engineering_i18n.md` | 国际化与本地化 | UTF-8/UTF-16、Satellite DLL、gettext、运行时语言切换 |
| 96 | `96_ProgrammingGUI_Engineering_Performance.md` | 性能优化 | 脏区优化、虚拟列表（LVS_OWNERDATA）、启动优化、双缓冲 |
| 97 | `97_ProgrammingGUI_Engineering_SysAPI.md` | 系统 API 集成 | 注册表、系统服务、COM 集成、剪贴板、文件对话框 |
| 98 | `98_ProgrammingGUI_Engineering_UIPatterns.md` | UI 工程模式 | 自定义控件设计模式、状态机、属性动画、配置管理 |
| 99 | `99_ProgrammingGUI_Engineering_OtherTopics.md` | 日志/错误处理/配置管理 | 结构化日志、错误传播、INI/JSON 配置、插件架构 |

### 不写

- **辅助功能（Accessibility）**：不覆盖 UIA / ARIA / 屏幕阅读器内容

### 各节内容要求

- 每节覆盖 Win32 / GTK / WinUI 3 三个框架的对应方案
- 提供跨框架对比代码
- 每节统一结构：概念讲解 → 代码结构 → 常见坑 → 小作业

## 代码示例（`src/tutorial/engineering/`）

| 目录 | 内容 |
|:-----|:-----|
| `01_thread_safe_log/` | 多线程安全日志控制台（批量化 UI 更新） |
| `02_dpi_aware/` | Per-Monitor V2 DPI 感知窗口 |
| `03_inno_setup/` | Inno Setup 安装脚本模板 |
| `04_virtual_list/` | 百万行虚拟列表（LVS_OWNERDATA） |
| `05_i18n_resource/` | 资源字符串多语言切换 |
| `06_raii_wrapper/` | RAII 封装工具集（GDI/COM/Handle） |

## 完成后更新

- [ ] `tutorial/index.md` — 添加第七篇表格
- [ ] `Roadmap.md` — 移除 7.6 辅助功能，重新编号，更新进度
- [ ] `CHANGELOG.md` — 记录变更
