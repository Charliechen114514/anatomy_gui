# 草稿 02 · 现代 Win32 / C++20 GUI 编程

> 目标：把 Win32 从“古老 API 教程”重写为现代 C++ 桌面程序的透明标本。传统 Win32 仍然保留，但新增一条现代范式主线：RAII、强类型封装、COM、DWM、DPI、Shell、WebView2、协程、取消与 UI dispatcher。

---

## 一、为什么要单独拆出现代 Win32

Win32 有两层价值：

1. **传统标本价值**：它让消息、窗口、绘制、控件通知全部可见。
2. **现代工程价值**：它仍然是 Windows 桌面软件最底层、最稳定、最可组合的基础。

现有 Win32 章节偏第一层。新板块要补第二层：

- 用 C++20 把 `HWND`、`HDC`、`HFONT`、`HANDLE`、COM 指针变成安全对象。
- 用协程和 dispatcher 改写“后台工作 + UI 回投”的套路。
- 用 Direct2D / DirectWrite / DWM / Shell / WebView2 组成现代桌面应用。
- 用 DPI、IME、a11y、主题、打包、测试定义“可交付”的最低线。

---

## 二、现代 Win32 的课程边界

### 保留传统 Win32

- `WinMain` / `wWinMain`。
- `RegisterClassEx`。
- `CreateWindowEx`。
- `WNDPROC`。
- `GetMessage` / `TranslateMessage` / `DispatchMessage`。
- `WM_PAINT` / `WM_COMMAND` / `WM_NOTIFY`。

### 新增现代层

- RAII 资源管理。
- 强类型消息路由。
- 窗口实例对象与生命周期。
- COM 初始化与智能指针。
- `IFileDialog`、Shell Item、Known Folder。
- DWM 深色模式、Mica、标题栏定制。
- Per-Monitor DPI v2。
- Direct2D / DirectWrite 作为默认现代绘制层。
- C++20 coroutine 与 UI dispatcher。
- WebView2 原生嵌入。
- UI 自动化、IME、高对比度。

---

## 三、建议章节族

### W20-00 · 从 C 风格 Win32 到现代 C++ 的桥

要点：

- 先承认 Win32 是 C API，不假装它“优雅”。
- 现代化不是把所有东西包成类，而是把所有权、错误、生命周期表达清楚。
- 传统 message loop 仍然保留，现代层只是建立在其上。

输出：

- 一张“旧写法 -> 新写法”对照表。

### W20-01 · RAII：先让资源不会泄漏

覆盖：

- `unique_handle` 思路。
- `DeleteObject` 管理 GDI 对象。
- `ReleaseDC` vs `DeleteDC`。
- `DestroyWindow` 的特殊性。
- `CloseHandle`、`CoTaskMemFree`。
- `wil`、`gsl::final_action`、自写 deleter 的取舍。

小项目：

- 把旧 GDI 画图示例改写为 RAII 版本。

### W20-02 · 类型安全的 Window 类

覆盖：

- `GWLP_USERDATA` 绑定 C++ 对象。
- `WM_NCCREATE` 存储 `this`。
- `static WndProc` 到实例方法。
- CRTP `BaseWindow<T>`。
- 多窗口实例。
- 窗口销毁、对象销毁、消息晚到的边界。

小项目：

- 一个可复用的 `Window` 基类。

### W20-03 · 消息路由的现代写法

覆盖：

- 大 `switch` 的优缺点。
- message map。
- `std::optional<LRESULT>` handler。
- `std::variant` 表示归一化事件。
- `WM_COMMAND` / `WM_NOTIFY` 的类型安全拆解。
- 子类化和控件事件转发。

小项目：

- 把按钮、编辑框、ListView 通知统一路由到 typed handler。

### W20-04 · Unicode、字符串与错误处理

覆盖：

- 全项目默认 `UNICODE` / `_UNICODE`。
- UTF-8 源码、UTF-16 Win32 边界。
- `std::wstring`、`std::u8string`、转换边界。
- `HRESULT`、`GetLastError`、异常、`std::expected` 的取舍。

小项目：

- 支持中文路径的文件打开与错误展示。

### W20-05 · COM 是现代 Windows GUI 的骨架之一

覆盖：

- `CoInitializeEx`。
- STA / MTA 与 UI 线程。
- `IUnknown`、引用计数、`QueryInterface`。
- `com_ptr` / `wil::com_ptr` / C++/WinRT。
- `HRESULT` 错误流。
- 为什么 `IFileDialog`、WebView2、WinRT 都绕不开 COM。

小项目：

- 用 `IFileDialog` 打开文件，并以 RAII 管理返回对象。

### W20-06 · 现代文件、Shell 与系统集成

覆盖：

- `IFileDialog` 替代旧 common dialog。
- `IShellItem`、Known Folder。
- 文件图标、属性、跳转列表、任务栏进度。
- 剪贴板、拖放、文件关联。

小项目：

- 一个小型“最近文件 + 拖放打开 + 任务栏进度”的壳。

### W20-07 · DWM、主题与 Windows 11 视觉

覆盖：

- 深色模式。
- Mica / Acrylic 的边界。
- 标题栏与非客户区。
- `WM_THEMECHANGED`。
- 高对比度和系统颜色。
- 不把“好看”建立在私有 hack 上。

小项目：

- 一个可随系统主题切换的窗口壳。

### W20-08 · DPI 与多显示器

覆盖：

- System DPI vs Per-Monitor DPI v1/v2。
- Manifest 与 runtime API。
- `WM_DPICHANGED`。
- 逻辑像素、物理像素、DIP。
- 字体、图标、布局、位图资源的缩放策略。

小项目：

- 跨显示器拖动时不糊、不跳、不截断的设置面板。

### W20-09 · Direct2D / DirectWrite 作为现代 2D 默认层

覆盖：

- 为什么不只用 GDI。
- D2D factory、render target、device context。
- DirectWrite 文本格式、文本布局、字体 fallback。
- GDI 互操作。
- resize、device lost、DPI。

小项目：

- 高 DPI 文本编辑区域的基本绘制。

### W20-10 · 输入系统：键盘、鼠标、Pointer、IME

覆盖：

- `WM_KEYDOWN` 不等于文本输入。
- `WM_CHAR`、dead key、快捷键。
- `WM_IME_*` composition。
- 鼠标捕获、双击、滚轮、高精度触控板。
- Pointer / touch / pen。

小项目：

- IME 事件日志器 + 可编辑文本框原型。

### W20-11 · UI 线程、后台任务与 PostMessage

覆盖：

- 为什么不能从后台线程直接改 UI。
- `PostMessage`、`SendMessage`、`PostThreadMessage` 的差别。
- 线程池与回投。
- 生命周期：窗口关了，后台任务还没结束怎么办。

小项目：

- 后台扫描目录，UI 增量显示结果。

### W20-12 · C++20 协程版 Win32 UI

覆盖：

- coroutine 不是线程。
- `co_await` timer。
- `co_await` background task。
- `resume_on_ui_thread`。
- cancellation token。
- 错误传播。
- 窗口销毁时取消挂起任务。

建议写一个教学 awaiter：

```cpp
co_await resume_background();
auto text = read_file(path);
co_await resume_ui(window);
editor.set_text(text);
```

小项目：

- 不冻结 UI 的异步文件加载器。

### W20-13 · WebView2：现代混合桌面

覆盖：

- Evergreen vs Fixed runtime。
- 初始化环境和 controller。
- native <-> JS 通信。
- 虚拟主机映射。
- 安全边界。
- WebView2 与 Electron / Tauri / CEF 的定位区别。

小项目：

- Win32 壳 + WebView2 Markdown 预览。

### W20-14 · 自动化测试、诊断与调试

覆盖：

- Spy++ / Inspect.exe。
- UI Automation smoke test。
- 日志、崩溃 dump。
- screenshot / golden testing 的可能性。
- 性能计数：消息频率、paint 次数、帧时间。

小项目：

- 一个可输出 UI 树、焦点、DPI、消息统计的 debug overlay。

---

## 四、关键示例项目

建议现代 Win32 板块不只配零散示例，而配 4 个贯穿项目：

| 项目 | 覆盖能力 |
|:---|:---|
| Modern Notepad | RAII、窗口类、菜单、IFileDialog、IME、异步读写 |
| Async Image Viewer | Shell、D2D、DPI、后台解码、缩放、取消 |
| WebView2 Markdown Preview | COM、WebView2、native/JS 通信、文件监控 |
| Desktop Inspector | 消息统计、DPI、控件树、UIA、性能观测 |

---

## 五、与旧章节的映射

| 旧内容 | 新定位 |
|:---|:---|
| ch0-2 基础窗口 | Act 1 传统 Win32 标本；Act 2 现代化重写 |
| ch3 DPI | Act 2 DPI 深化 + Act 6 产品化 |
| ch4-8 控件 | Act 1 系统控件；Act 2 类型安全通知路由 |
| ch9-17 对话框/资源 | Act 1 传统资源；Act 2 IFileDialog/Manifest/Shell |
| ch18-25 GDI | Act 1/5 传统绘制；Act 2 逐步过渡到 D2D |
| ch52-60 进阶消息 | Act 1 传统机制；Act 2 生命周期、安全封装 |
| MiniUI 协程章节 | Act 2 Win32 协程与 Act 3 框架协程互相回链 |

---

## 六、必须写清楚的坑

- `HWND` 的销毁不等于 C++ 对象立即安全消失。
- `SendMessage` 可能重入。
- 模态对话框会产生嵌套消息循环。
- 后台线程不能碰 UI 对象。
- COM apartment 选择会影响异步和 WebView2。
- DPI 感知必须从 manifest 与启动阶段开始。
- IME 不是键盘事件的简单变体。
- DWM 私有属性和系统版本差异要谨慎。
- 协程挂起后，捕获的 `this` 可能悬空。

---

## 七、本板块的产出标准

- 每个核心章给一个可编译示例。
- 所有 handle / COM 示例必须展示所有权。
- 每个异步示例必须展示取消与窗口关闭。
- 至少一个示例支持中文路径、中文输入、高 DPI。
- 至少一个示例展示 UIA 或可访问性检查。
