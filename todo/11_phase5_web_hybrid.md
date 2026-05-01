# Phase 5 — Web 混合方案（第六篇）

> WebView2 + CEF，移除 Electron 和 Tauri

## 前置依赖

- Phase 4（COM 基础 ch76，WebView2 强依赖 COM）

## 教程文件

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 81 | `81_ProgrammingGUI_WebHybrid_Overview.md` | 混合方案选型概览 | 原生 vs Web 权衡矩阵、适用场景、决策树 |
| 82 | `82_ProgrammingGUI_WebView2.md` | WebView2 深度集成 | 架构、初始化流程、双向通信（C++↔JS）、Evergreen vs Fixed |
| 83 | `83_ProgrammingGUI_WebView2_Advanced.md` | WebView2 进阶 | 虚拟主机映射、CDP 协议、打印、Cookie 管理、多实例 |
| 84 | `84_ProgrammingGUI_CEF.md` | CEF 集成 | 进程模型、CefApp/CefClient/CefBrowserHost、JS↔C++ 桥接 |
| 85 | `85_ProgrammingGUI_CEF_Advanced.md` | CEF 进阶 | 自定义协议/资源拦截、离屏渲染、多进程调试 |

### 各节内容要求

- WebView2 以 Win32 原生嵌入为主（不是 WinUI 3 封装）
- CEF 展示完整的最小浏览器实现
- 两者对比选型指南
- 每节统一结构：概念讲解 → 代码结构 → 常见坑 → 小作业

## 代码示例

### `src/tutorial/webview2/`

| 目录 | 内容 | 依赖 |
|:-----|:-----|:-----|
| `01_wv2_hello/` | WebView2 最小嵌入 Win32 窗口 | WebView2 SDK (NuGet) |
| `02_wv2_js_bridge/` | C++↔JS 双向通信完整示例 | WebView2 SDK |
| `03_wv2_virtual_host/` | 虚拟主机映射 + 本地资源加载 | WebView2 SDK |

### `src/tutorial/cef/`

| 目录 | 内容 | 依赖 |
|:-----|:-----|:-----|
| `01_cef_hello/` | CEF 最小集成（CefInitialize → CefRunMessageLoop） | CEF binary distribution |
| `02_cef_browser/` | 完整浏览器（地址栏 + 前进/后退 + JS 桥接） | CEF binary distribution |

> 第三方依赖不打包，提供集成指导文档。

## 完成后更新

- [ ] `tutorial/index.md` — 添加第六篇表格
- [ ] `Roadmap.md` — 移除 Electron/Tauri 章节，更新进度
- [ ] `CHANGELOG.md` — 记录变更
