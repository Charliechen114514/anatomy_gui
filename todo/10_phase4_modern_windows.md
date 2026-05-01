# Phase 4 — 现代 Windows 技术（第五篇）

> COM 基础 + WinUI 3 完整覆盖

## 前置依赖

- Phase 1（Win32 进阶，提供 COM 使用场景上下文）

## 教程文件（`tutorial/native_win32/` 或新建目录）

> COM 和 WinUI 3 教程延续当前编号风格。

| 编号 | 文件名 | 标题 | 说明 |
|:-----|:-------|:-----|:-----|
| 76 | `76_ProgrammingGUI_COM_Basics.md` | COM 基础 | IUnknown、引用计数、工厂模式、C++/WinRT 智能指针（com_ptr/wil::com_ptr） |
| 77 | `77_ProgrammingGUI_WinUI3_Intro.md` | WinUI 3 与 Windows App SDK 入门 | 演进路径、WinAppSDK、部署方式、C++/WinRT 工程结构 |
| 78 | `78_ProgrammingGUI_WinUI3_XAMLBinding.md` | XAML 标记语言与数据绑定 | XAML 语法、{Binding} vs {x:Bind}、INotifyPropertyChanged |
| 79 | `79_ProgrammingGUI_WinUI3_WinRT.md` | WinRT API 使用 | C++/WinRT、IAsyncOperation、co_await、常用 WinRT API |
| 80 | `80_ProgrammingGUI_WinUI3_MSIX.md` | MSIX 打包与部署 | 包结构、Capabilities、打包方式、调试部署 |

### COM 章节定位

COM 是 WebView2（第六篇）、Win32 拖放 OLE（Phase 1 ch56）的前置知识。作为独立章节放在第五篇开头，后续章节引用即可。

## 代码示例

### `src/tutorial/com_basics/`

| 目录 | 内容 | 链接库 |
|:-----|:-----|:-------|
| `01_com_hello/` | COM 基础：IUnknown/QueryInterface 最小示例 | ole32.lib |
| `02_com_smartptr/` | C++/WinRT com_ptr vs wil::com_ptr_ptr 对比 | ole32.lib, windowsapp.lib |

### `src/tutorial/winui3/`

| 目录 | 内容 | 依赖 |
|:-----|:-----|:-----|
| `01_winui3_hello/` | WinUI 3 最小窗口 + NavigationView | Windows App SDK |
| `02_xaml_binding/` | {x:Bind} 数据绑定示例 | Windows App SDK |
| `03_winrt_filepicker/` | 异步 FileOpenPicker + co_await | Windows App SDK |
| `04_msix_packaging/` | MSIX 打包配置（AppxManifest.xml） | Windows App SDK |

> WinUI 3 示例需要 Windows 10 1809+ 和 Windows App SDK。

## 完成后更新

- [ ] `tutorial/index.md` — 添加第五篇表格
- [ ] `Roadmap.md` — 更新第五篇完成度 0% → 100%
- [ ] `CHANGELOG.md` — 记录变更
