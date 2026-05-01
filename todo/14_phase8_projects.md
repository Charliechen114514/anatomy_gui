# Phase 8 — 综合实战项目（第八篇）

> 跨平台 Markdown 编辑器，三框架分别实现

## 前置依赖

- Phase 4（WinUI 3 + COM）
- Phase 5（WebView2，Win32 版本依赖 WebView2）
- Phase 3（GTK）

## 教程文件

| 编号 | 文件名 | 标题 |
|:-----|:-------|:-----|
| 100 | `100_ProgrammingGUI_Project_MarkdownEditor_Overview.md` | 项目概述与功能规格 |
| 101 | `101_ProgrammingGUI_Project_MarkdownEditor_Win32.md` | Win32 + WebView2 实现 |
| 102 | `102_ProgrammingGUI_Project_MarkdownEditor_GTK.md` | GTK/gtkmm 实现 |
| 103 | `103_ProgrammingGUI_Project_MarkdownEditor_WinUI3.md` | WinUI 3 实现 |

### 功能规格

- 左侧编辑区 + 右侧实时预览（500ms 防抖刷新）
- 文件操作：新建、打开、保存、另存为、最近文件列表（MRU）
- 工具栏 + 菜单栏 + 状态栏（字数、行号、编码、光标位置）
- Markdown 语法高亮
- 系统主题跟随（深色/浅色模式自动切换）

### 各版本侧重

| 框架 | 侧重展示的知识点 |
|:-----|:----------------|
| Win32 + WebView2 | 第一篇 + 第六篇综合；原生窗口 + Web 预览通信 |
| GTK/gtkmm | 第四篇综合；Linux 原生集成、CSS 主题 |
| WinUI 3 | 第五篇综合；XAML 数据绑定、Fluent Design、MSIX 打包 |

## 代码示例（`src/tutorial/projects/markdown_editor/`）

| 目录 | 内容 | 平台 |
|:-----|:-----|:-----|
| `win32_webview2/` | 完整可编译项目 | Windows |
| `gtk/` | 完整可编译项目 | Linux |
| `winui3/` | 完整可编译项目 | Windows |
| `shared_spec/` | 共享的测试用例/规格文件 | N/A |

### 验收检查清单

- [ ] 大文件（> 10MB Markdown）打开不卡顿（异步加载 + 虚拟列表）
- [ ] 多显示器 DPI 缩放正确
- [ ] 中文输入法（IME）字符输入无乱码
- [ ] 关闭未保存文档时弹出确认
- [ ] 安装包制作完成，可在干净系统上部署运行

## 完成后更新

- [ ] `tutorial/index.md` — 添加第八篇表格
- [ ] `Roadmap.md` — 移除 Qt/wxWidgets 版本，更新进度
- [ ] `CHANGELOG.md` — 记录变更
