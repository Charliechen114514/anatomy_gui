# Win32 必备知识 · 穿插清单（M1）

> 这一份**不是** Win32 教程，是穿插弹药：M1 每一篇写到哪个代码点，就从这里取一条就地讲。
> 相关：[architecture.md](architecture.md)（架构契约）· [positioning.md](positioning.md)

## 四条讲法约定

1. **就地讲** —— 第一次用到才讲，一两句，绝不展开成章节。一旦单独成章，教程就退回成 Win32 手册了。
2. **必须挂在代码点上** —— 每条都注明它出现在哪个文件/函数。没有代码点的知识不收。
3. **只讲"为什么此刻必须知道"** —— API 全集交给官方文档；想深挖的，末尾给一个 native_win32 的链接。
4. **坑优先** —— 一条知识如果没有坑，就不值得穿插。穿插的价值在于"不提前说，读者一定会卡住"。

---

## A · 工程与入口（M1 第 0 天）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| `wWinMain` 与 `hInstance` | `Application` 构造需要实例句柄 | Windows 程序的入口是 `wWinMain`；`hInstance` 是本模块的内存基址，注册窗口类时要带上 | 0–1 |
| 子系统与黑框 | 不设就会多弹一个控制台窗 | CMake 里 `add_executable(gui WIN32 ...)`，等价于链接 `/SUBSYSTEM:WINDOWS` | — |
| `UNICODE` / `/utf-8` / `L""` | 中文读者的第一道坎 | 一律用宽字符 API（`...W`），源码编译加 `/utf-8`，字面量写 `L"..."`；混用 `TCHAR` 迟早在某个编译器上炸 | 0 |
| manifest | 不做就没 DPI 感知，按钮还是 Windows 95 长相 | 一份 manifest 声明两件事：DPI 感知级别 + 依赖 Common Controls v6（视觉样式） | **素材缺，得新写** |
| `InitCommonControlsEx` | 不调用，`LISTVIEW` 直接创建失败 | 在 `wWinMain` 里 `InitCommonControlsEx({ICC_LISTVIEW_CLASSES \| ICC_BAR_CLASSES})` | 6 |

## B · 窗口与消息（platform 层）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| 注册窗口类 | 每个自建窗口都得先注册 | `WNDCLASSEXW` 五个必填项：`lpfnWndProc`、`hInstance`、`lpszClassName`、`hCursor`、`hbrBackground`；类名进程内唯一 | 1–2 |
| `CreateWindowExW` 的 `lpParam` | OOP 包装窗口的第一个坑 | `WM_NCCREATE` 在 `CreateWindowExW` **返回之前**就发出去了，所以 `this` 必须通过 `CREATESTRUCT::lpCreateParams` 带进去 | 2、10 |
| `HWND` 的身份 | 事件路由全靠它找回对象 | HWND 是不透明句柄，而且**会被复用**：窗口销毁后同一个值可能分给新窗口。所以它只能当临时键，不能用来判断"还是不是原来那个控件" | **素材缺（散落）** |
| 消息循环 | `Application::run()` 的本体 | `GetMessageW` → `TranslateMessage` → `DispatchMessageW`；`GetMessage` 出错返回 `-1`、退出返回 `0`，写 `while (GetMessage(...))` 会把 `-1` 当真 | 2–4 |
| `WndProc` 与 `DefWindowProcW` | 所有消息的落脚点 | 不处理的消息必须交给 `DefWindowProcW`；`WM_NCCREATE` 返回 `FALSE` 会直接中止窗口创建 | 2–5 |
| 窗口生命周期 | 决定对象何时能安全销毁 | `WM_NCCREATE → WM_CREATE → … → WM_CLOSE → WM_DESTROY → WM_NCDESTROY`，`WM_NCDESTROY` 之后才算干净 | 7、10 |
| `WM_SIZE` | 布局的唯一触发源 | 新尺寸在 `lParam` 的低/高 16 位；最小化时尺寸是 0，要用 `SIZE_MINIMIZED` 挡掉，否则布局会算出 0 宽 | **素材缺（散落）** |
| 客户区 vs 窗口矩形 | 布局的坐标起点 | 布局一律用 `GetClientRect`（客户区），不要用 `GetWindowRect`（那含标题栏和边框） | 1–2 |
| `WM_PAINT` 与 `BeginPaint` | 绘制入口 | `BeginPaint` 校验脏区并给你 HDC，`EndPaint` 收尾；两者不配对会刷屏死循环 | 18、23 |
| 背景擦除与闪烁 | 验收第 2 条（不闪烁） | `WM_ERASEBKGND` 先擦背景再让你画 → 闪；返回非 0 表示"我擦了"，或者干脆上双缓冲 | 23、60 |
| 子窗口与 `WS_CLIPCHILDREN` | 混合树的父窗口必须加 | 父窗口不加 `WS_CLIPCHILDREN`，它重画时会盖到子控件上，留下残影 | **素材缺** |

## C · 控件与事件桥（framework 层 · NativeWidget）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| 系统预注册的控件类 | `NativeWidget` 要创建子 HWND | `BUTTON` / `EDIT` / `STATIC` / `WC_LISTVIEWW` 都不用自己注册，`CreateWindowExW` 直接拿类名用 | 4、8 |
| `hMenu` 参数的双重身份 | 子窗口的"控件 ID"从这来 | `CreateWindowExW` 的 `hMenu`：顶层窗口当菜单，**子窗口当控件 ID**（一个整数被强转成 `HMENU`）—— 反直觉，但必须知道 | 4 |
| `WM_COMMAND` | 按钮点击进入框架的唯一通道 | `LOWORD(wParam)` = 控件 ID，`HIWORD(wParam)` = 通知码，`lParam` = 控件 HWND；三个信息凑齐才能定位是谁发生了什么 | 4–5 |
| `WM_NOTIFY` | ListView 这类控件不走 `WM_COMMAND` | `lParam` 指向 `NMHDR`（`hwndFrom` / `idFrom` / `code`），`code` 决定后面该转成哪个结构体 | 5–7 |
| 控件字体 `WM_SETFONT` | 不设的话中文控件字又小又丑 | 控件默认字体是古董 `SYSTEM_FONT`，创建后必须 `SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE)` | 4、8 |
| 父子窗口坐标 | `arrange()` 里要调 `SetWindowPos` | 子窗口坐标是**相对父窗口客户区**的，不是屏幕坐标 | 3–4 |
| 控件 ID → 对象映射 | 事件桥接的全部秘密 | 给每个 `NativeWidget` 分配唯一 ID，收到 `WM_COMMAND` 后查表找回对象 —— M1 事件层就这么点东西 | **素材缺** |
| 键盘焦点 | 输入框能不能打字 | `SetFocus` 决定谁收键盘消息；`WM_SETFOCUS`/`WM_KILLFOCUS` 是控件自己的事，框架只负责转移 | 4、52 |

## D · 上屏（render 层 · GdiPainter）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| `HDC` 与 GDI 对象 | `GdiPainter` 的全部家当 | HDC 是个绘制状态机：`SelectObject` 换笔刷/字体，用完 `DeleteObject`。**GDI 对象是进程级配额**（默认上限约 1 万），漏一个就少一个 | 18–19 |
| 每帧不要 `CreateFont` | M1 的性能底线 | 字体/画笔/位图必须在第一帧建好并缓存；每帧创建 GDI 对象是教科书级灾难 | 19 |
| 文本绘制与测量 | 布局依赖测量 | `DrawTextW` 画、`DT_CALCRECT` 或 `GetTextExtentPoint32W` 量；**测量和绘制必须用同一字体**，否则布局会跳 | 21、32 |
| 双缓冲 | 不闪的最省事做法 | 先在内存 DC 上画完，再一次性 `BitBlt` 到窗口 | 23 |
| `COLORREF` 是 BGR | 颜色转换写错的经典坑 | `RGB(r,g,b)` 生成的是 `0x00BBGGRR`；GDI 不带 alpha 通道 | 20 |
| 类背景刷 | `WM_ERASEBKGND` 的另一半 | `hbrBackground` 是"系统替你擦背景"；自绘之后应当设成 `nullptr` 并自己擦 | 60 |
| `PAINTSTRUCT::rcPaint` | 将来做局部重画时脏区从哪来 | `rcPaint` 就是本次要画的矩形；M1 忽略它整窗重画，M2 的 `Painter::clip` 就来自它 | 23、60 |

## E · DPI 与坐标（platform 层 · Window）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| DPI 感知声明 | 验收第 4 条（高 DPI 不糊） | 必须在 manifest 或进程启动时声明 `PER_MONITOR_AWARE_V2`；不声明的话系统直接拉伸你的窗口，字全糊 | 3 |
| 逻辑单位与 `GetDpiForWindow` | 坐标契约二 | 布局用 DIP，渲染时乘 `GetDpiForWindow(hwnd) / 96.0`；这个比例只有 `Window` 持有 | 3 |
| `WM_DPICHANGED` | 拖到另一块屏要重排 | 消息里带一个建议的新窗口矩形（`lParam` 是 `RECT*`），必须 `SetWindowPos` 照它走，否则窗口会跳 | 3 |
| 屏幕 ↔ 客户区换算 | 命中测试 | `ScreenToClient` / `ClientToScreen`；鼠标消息给的已经是客户区坐标，但拖放和多窗口场景要自己换 | 50 |

## F · 错误与生命周期（framework 层）

| 知识点 | 为什么此刻需要 | 最小讲法 | 深挖 |
|:---|:---|:---|:---|
| `GetLastError` | 所有 Win32 调用失败后的唯一线索 | 失败就是返回 `FALSE`/`NULL`，原因在 `GetLastError()`；必须在下一个成功调用之前读走它 | **素材缺（散落）** |
| 句柄的所有权 | `Window` 的析构写什么 | `DestroyWindow` 销毁 HWND；顶层窗口销毁会让消息循环退出（`PostQuitMessage`）——这是两件事 | 7 |
| 为什么不能裸持 HWND | 生命周期契约四 | 框架对象比 HWND 活得久或死得早都会出事，所以 HWND 只允许住在 `platform::Window` 里，上层只见框架对象 | **素材缺** |

## G · 素材缺口（61 章里没有，M1/M2 必须新写）

这几条我按关键词扫过全库，确认是空的，所以不是"另找一章引用"，而是**真得写**：

| 主题 | 缺口程度 | 用在哪 |
|:---|:---|:---|
| 视觉样式与主题绘制（`OpenThemeData` / `DrawThemeBackground`） | 全库 **0 处** | M1 讲"原生控件的像素到底谁画的"；M2 自绘时要模仿 |
| DWM 重定向表面（redirection surface） | 全库 **0 处** | M2 第 2 档换 DComp 的前置知识 |
| `WS_EX_COMPOSITED` / `BufferedPaint` | 各 **0 处** | M2 第 2 档讲"系统自己的双缓冲" |
| `WS_CLIPCHILDREN` | **0 处** | M1 混合树立刻要用 |
| UIA 暴露（`WM_GETOBJECT`） | **0 处** | M2 第 5 档让无障碍自己还 |

---

## 怎么用这张表

写 M1 任何一篇之前，先列出这篇会碰到的代码点，再从表里取对应当条：

- 就地讲**一两句**，讲清"为什么此刻必须知道"和那个坑
- 末尾挂一个"想深挖看 native_win32 第 N 章"的链接
- 表里标了**素材缺**的，说明这一条在写完之前，读者没有可深挖的地方 —— 这些点要优先补

一句话：**穿插的知识全部挂在代码点上，读者永远是在解决问题时顺手学会 Win32，而不是先啃完 Win32 再来写代码。**
