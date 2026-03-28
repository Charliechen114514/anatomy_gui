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


# GUI 编程系列 · 完整路线图（细化版）

> **定位**：面向掌握 C++ 基础语法的读者，循序渐进，概念先行，覆盖 Win32 / Qt / WinUI / GTK / wxWidgets / Web 前端，配套 GitHub 代码仓库与博客文章。
>
> **每节统一结构**：概念讲解 → 代码结构 → 常见坑 → 小作业

---

## 总体结构概览

```
第〇篇  前言与准备              ← 世界观建立
第一篇  Win32 原生编程          ← 地基（进行中 75%）
第二篇  GUI 核心概念            ← 跨框架通识
第三篇  图形渲染（独立系列）    ← GDI → Direct2D → D3D11 → D3D12
第四篇  跨平台框架              ← Qt / wxWidgets / GTK
第五篇  现代 Windows 技术       ← WinUI 3 / WebView2
第六篇  Web 混合方案            ← Electron / CEF / Tauri
第七篇  工程化专题              ← 多线程、DPI、打包、i18n
第八篇  综合实战项目            ← 贯穿全系列的收尾
```

---

## 第〇篇 · 前言与准备

### 0.1 为什么要学 GUI 编程？
**概念讲解**
- GUI 程序的四大要素：窗口、消息、渲染、控件
- 与 Web 前端、移动端的横向对比（事件模型、渲染树、布局系统的共性与差异）
- C++ GUI 的生态现状：原生性能 vs 开发效率的权衡地图

**代码结构**
- 最小 Win32 程序骨架（30 行）：展示 WinMain → RegisterClass → CreateWindow → 消息循环的完整轮廓
- 对比同等功能的 Qt 程序（10 行），引发「封装层次」的思考

**常见坑**
- 误以为 GUI 编程 = 拖控件；忽略消息循环本质
- 混淆「渲染」与「布局」两个独立关注点

**小作业**
- 能说出「一次鼠标点击」从硬件中断到 WM_LBUTTONDOWN 的完整路径（不要求代码，文字描述即可）

---

### 0.2 开发环境搭建
**概念讲解**
- Windows 工具链全景：MSVC（cl.exe）/ MinGW-w64 / Clang-cl 的区别与适用场景
- Visual Studio 2022 项目类型选择：Win32 Project vs Empty Project vs CMake Project

**代码结构**
- 一份可复用的 CMakeLists.txt 模板，支持 Win32 / Qt / GTK 多目标切换
- `.clang-format` + `.editorconfig` 统一代码风格配置

**常见坑**
- MSYS2 的 MINGW64 / UCRT64 / CLANG64 三个子系统不能混用
- Unicode 字符集与多字节字符集设置不一致导致编译报错
- Spy++ 找不到：需在 VS 安装时勾选「C++ MFC」组件

**小作业**
- 搭建环境，编译并运行仓库中的 `hello_window` 示例，截图提交

---

### 0.3 本系列的学习方法
**概念讲解**
- 如何阅读 MSDN/Microsoft Learn：函数签名 → 参数说明 → 返回值 → Remarks（这才是精华）→ See Also
- 「先跑起来，再搞懂」与「先搞懂，再写代码」两种路线的适用阶段

**代码结构**
- 配套 GitHub 仓库目录结构说明（每篇独立目录，每节独立可编译子项目）
- Issue / Discussion 提问模板：最小复现代码 + 环境信息

**常见坑**
- 直接搜索中文博客得到的 Win32 示例大量使用已废弃 API 或错误模式

**小作业**
- 在 MSDN 上查找 `CreateWindowEx` 的文档，找出 `dwExStyle` 中 `WS_EX_LAYERED` 的作用并用一句话概括

---

## 第一篇 · Win32 原生编程

> 本篇进度 75%，已发布章节保持原结构，以下列出**待补充**与**进阶**部分的细化提纲。

---

### 需补充：1.2 消息机制补充

**概念讲解**
- `SendMessage` vs `PostMessage`：同步调用栈 vs 异步队列投递，各自的死锁风险
- 消息队列的两层结构：发送消息队列（sent-message queue）与投递消息队列（posted-message queue）
- 其他消息函数：`SendNotifyMessage`（不等返回）、`SendMessageCallback`（异步回调）、`PostThreadMessage`（线程间通信）
- `InSendMessage` / `ReplyMessage`：处理跨线程 SendMessage 时避免死锁的正确姿势

**代码结构**
```
// 跨线程安全通知模式
// 主线程：HWND hWnd 已知
// 工作线程：PostMessage(hWnd, WM_USER+1, wParam, lParam)
// 主线程 WndProc：case WM_USER+1 → 更新 UI
```
- 完整示例：工作线程计算进度 → PostMessage → 主线程更新进度条

**常见坑**
- 在工作线程中调用 `SendMessage` 到主线程，而主线程正在等待工作线程 → 经典死锁
- `PostMessage` 投递的 `lParam` 传递堆指针时，消息处理前对象被析构

**小作业**
- 写一个程序：子线程每秒递增计数器，通过消息通知主窗口刷新显示；要求不使用全局变量共享状态

---

### 需补充：1.3 常用系统消息补充

**概念讲解**
- 输入消息扩展：`WM_MOUSEWHEEL`（WHEEL_DELTA 单位）、`WM_TOUCH`（多点触控）、`WM_POINTER`（统一指针模型，Win8+）、`WM_INPUT`（Raw Input，游戏/高精度场景）
- 窗口行为消息：`WM_GETMINMAXINFO`（限制窗口最小/最大尺寸）、`WM_NCHITTEST`（自定义标题栏拖拽区域）、`WM_WINDOWPOSCHANGING/CHANGED`
- 绘制相关：`WM_ERASEBKGND`（背景擦除，自绘窗口必须处理）、`WM_PRINTCLIENT`（BitBlt 时触发，动画截图场景）

**代码结构**
- `WM_NCHITTEST` 实现无边框窗口可拖拽区域的完整代码模板
- Raw Input 注册与读取的最小示例

**常见坑**
- 处理 `WM_ERASEBKGND` 时不返回 1 → 与 `WM_PAINT` 叠加导致闪烁
- `WM_TOUCH` 与 `WM_MOUSEWHEEL` 在触控屏上会同时触发，需要 `RegisterTouchWindow` 后用 `CloseTouchInputHandle` 正确释放

**小作业**
- 实现一个窗口：鼠标滚轮控制背景颜色在色相环上滚动，无边框但可拖动

---

### 1.7 Win32 进阶

#### 1.7.1 子类化（Subclassing）与超类化（Superclassing）

**概念讲解**
- 子类化原理：用 `SetWindowSubclass` 替换窗口过程指针，形成调用链
- `SetWindowSubclass` vs 老式 `SetWindowLongPtr(GWLP_WNDPROC)`：前者支持多级子类化、自动清理
- 超类化：`GetClassInfo` → 修改 `WNDCLASSEX.lpfnWndProc` → `RegisterClass` 注册新类，适合批量定制同类控件

**代码结构**
```cpp
// 子类化 Edit 控件，拦截 Enter 键
SetWindowSubclass(hEdit, EditSubclassProc, 0, 0);

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) { /* ... */ }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
```

**常见坑**
- 窗口销毁前忘记调用 `RemoveWindowSubclass` → 访问悬空指针
- 在子类过程中调用 `CallWindowProc` 而非 `DefSubclassProc` → 跳过其他子类层

**小作业**
- 子类化一个 `ListBox`，实现奇偶行交替背景色（Owner-Draw 替代方案对比）

---

#### 1.7.2 Hook 机制

**概念讲解**
- Hook 类型速查：`WH_KEYBOARD_LL` / `WH_MOUSE_LL`（系统全局低级钩子，无需注入）vs `WH_CALLWNDPROC`（进程内钩子）
- Hook 链：多个 Hook 串联，必须调用 `CallNextHookEx`
- 全局钩子的 DLL 注入机制（仅作原理说明，不鼓励实际使用）

**代码结构**
- 全局键盘钩子监听截图键（Print Screen）的完整示例
- Hook 安装在独立线程 + 消息循环的标准结构

**常见坑**
- 低级钩子回调耗时过长（> 系统超时阈值约 300ms）→ Windows 自动移除钩子
- 忘记在程序退出时 `UnhookWindowsHookEx`

**小作业**
- 实现一个「按键记录器」（仅记录自身进程窗口的输入），展示 Hook 与 Subclass 两种方案的对比

---

#### 1.7.3 系统托盘

**概念讲解**
- `Shell_NotifyIcon` 的四个操作：`NIM_ADD` / `NIM_MODIFY` / `NIM_DELETE` / `NIM_SETVERSION`
- `NOTIFYICONDATA` 关键字段：`hWnd`（接收回调消息）、`uCallbackMessage`、`hIcon`、`szTip`
- 托盘气泡通知（`NIIF_INFO` / `NIIF_WARNING`）vs 现代 Toast 通知的选型

**代码结构**
- 最小托盘程序：图标 + 右键菜单 + 双击还原窗口的完整模板
- `WM_TASKBARCREATED` 注册：任务栏重启后自动重建托盘图标

**常见坑**
- 使用 `NOTIFYICONDATA` 时 `cbSize` 字段必须与系统版本对应（用 `sizeof` 即可，但要注意结构体版本）
- 托盘图标 `hIcon` 是共享资源，程序退出前无需（也不应）`DestroyIcon` 系统图标

**小作业**
- 实现「番茄钟」托盘程序：倒计时结束时弹出气泡通知，右键菜单可暂停/重置/退出

---

#### 1.7.4 拖放（Drag & Drop）

**概念讲解**
- 两套机制对比：`WM_DROPFILES`（简单文件拖入，`DragAcceptFiles`）vs OLE `IDropTarget`（完整拖放协议，支持自定义数据格式）
- OLE 拖放四角色：`IDropSource`、`IDropTarget`、`IDataObject`、`IEnumFORMATETC`
- `CLIPFORMAT` 标准格式：`CF_HDROP`（文件列表）、`CF_UNICODETEXT`、自定义注册格式

**代码结构**
```cpp
// 简单文件拖入
DragAcceptFiles(hWnd, TRUE);
// WndProc:
case WM_DROPFILES: {
    UINT count = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; i++) { /* DragQueryFile(hDrop, i, ...) */ }
    DragFinish((HDROP)wParam);
}
```
- `IDropTarget` 最小实现骨架（COM 接口实现模板）

**常见坑**
- 64 位程序接收来自 32 位程序的拖放需要特殊处理（`ChangeWindowMessageFilterEx`）
- 忘记调用 `OleInitialize` → `IDropTarget` 注册失败但无错误提示

**小作业**
- 实现文件拖入窗口后，在窗口中显示文件路径列表；进阶：高亮显示拖入悬停状态

---

#### 1.7.5 定时器

**概念讲解**
- `SetTimer` / `WM_TIMER`：基于消息队列，精度约 15ms（受系统定时器分辨率限制）
- 高精度定时：`timeSetEvent`（多媒体定时器）vs `CreateWaitableTimer` vs 独立线程 `Sleep`
- `QueryPerformanceCounter` 用于时间测量（不是定时触发）

**代码结构**
- `WM_TIMER` 动画循环的标准结构：`InvalidateRect` 触发重绘
- 高精度定时器线程 → `PostMessage` 到主线程的解耦模式

**常见坑**
- `WM_TIMER` 不是实时消息，消息队列阻塞时会积压合并
- `KillTimer` 后仍可能收到一条残留的 `WM_TIMER`，需要用标志位防御

**小作业**
- 实现一个模拟时钟（时分秒针），使用 `WM_TIMER` 每秒更新，GDI 绘制

---

### 阶段小项目：纯 Win32 文本编辑器

**功能规格**
- 菜单栏（文件、编辑、帮助菜单）
- 工具栏（新建、打开、保存、剪切、复制、粘贴）
- 状态栏（行号、列号、文件名、修改状态）
- 核心功能（Edit/RichEdit 控件、文件对话框、快捷键加速表）
- 可选扩展（查找/替换对话框、行号显示边栏、语法高亮 via RichEdit）

**阶段目标检验**
- 能正确处理大文件（> 1MB）时的性能问题
- 文件编码自动检测（UTF-8 BOM / UTF-16 LE / ANSI）
- 未保存时关闭窗口弹出确认对话框

---

## 第二篇 · GUI 核心概念（跨框架通识）

### 2.1 事件驱动编程模型

**概念讲解**
- 事件循环的通用模型：`while(getEvent) → dispatch → handler → 返回循环`
- 事件 vs 消息 vs 信号：三个术语在不同框架中的映射关系
- 同步事件（在调用栈内完成）vs 异步事件（投递到队列延迟处理）的选择依据

**代码结构**
- Win32 消息循环 / Qt `exec()` / GTK `gtk_main()` 三者伪代码对比，抽象出共同骨架

**常见坑**
- 在事件处理函数中做耗时操作 → 卡死 UI 的本质原因
- 递归进入事件循环（如在处理消息时调用 `MessageBox`）的副作用

**小作业**
- 画出「用户点击按钮 → 弹出对话框」这一交互在 Win32 / Qt 中的完整调用链时序图

---

### 2.2 控件树与布局系统

**概念讲解**
- 父子控件树的普遍性：Win32 HWND 树 / Qt QObject 树 / DOM 树的同构性
- 三种布局模型对比：绝对布局（Win32）、盒模型流式布局（Web/GTK）、约束布局（Qt/SwiftUI/Android）
- 布局引擎的工作流：测量（measure）→ 布置（arrange）→ 渲染（render）

**代码结构**
- 同一个「登录表单」用 Win32 绝对坐标 / Qt QFormLayout / GTK GtkGrid 三种方式实现，代码并排对比

**常见坑**
- Win32 绝对布局不响应 `WM_SIZE` → 窗口缩放后控件不跟随
- Qt 布局中 `setSizePolicy` 设置不当导致控件无法伸缩

**小作业**
- 用 Win32 实现一个响应式布局：窗口缩放时，内部三个按钮始终均匀分布在底部

---

### 2.3 MVC / MVP / MVVM 模式

**概念讲解**
- 为什么 GUI 需要架构模式：数据与视图耦合后的维护噩梦
- MVC（Model-View-Controller）：Controller 协调，View 被动
- MVP（Model-View-Presenter）：View 接口化，方便单元测试；Win32/GTK 手动实现的首选
- MVVM（Model-View-ViewModel）：数据绑定驱动，WinUI/Qt Quick 的原生范式

**代码结构**
- Win32 手动 MVP：定义 `IView` 接口 → Presenter 持有接口指针 → WndProc 实现接口
- Qt Model/View：`QAbstractItemModel` 子类 + `QListView` 绑定的最小示例

**常见坑**
- ViewModel 直接操作 UI 控件 → MVVM 的核心禁忌
- Win32 中 Presenter 持有 HWND 裸指针，窗口销毁后未清空

**小作业**
- 用 MVP 模式重构 1.7.5 的模拟时钟：Model 管理时间数据，Presenter 驱动更新，View 只负责绘制

---

### 2.4 绘制流水线

**概念讲解**
- CPU 绘制（GDI/GDI+）vs GPU 绘制（Direct2D/D3D）：数据在哪里，瓶颈在哪里
- 立即模式（Immediate Mode）：每帧全部重绘；保留模式（Retained Mode）：场景图维护差量更新
- 脏区（Dirty Region）机制：`InvalidateRect` 的精确用法与 `ValidateRect` 的配合

**代码结构**
- 脏区最小化示例：只在鼠标移动时 `InvalidateRect` 矩形选区，而非整个窗口

**常见坑**
- 在 `WM_PAINT` 之外直接绘制（`GetDC` + 绘制 + `ReleaseDC`）未进入脏区系统，重叠窗口时内容会被覆盖

**小作业**
- 实现一个画板程序，要求：鼠标拖拽绘制线条，最小化脏区刷新（只刷新新增线段的包围盒）

---

### 2.5 字体与文字渲染

**概念讲解**
- 字体度量（Metrics）：ascent / descent / leading / baseline 的几何含义，与排版的关系
- 光栅化 vs 向量：GDI ClearType / DirectWrite / FreeType 的渲染差异
- Unicode 文字渲染的复杂性：双向文本（BiDi）、连字（Ligature）、组合字符、Emoji

**代码结构**
- GDI `GetTextMetrics` 实现多行文字垂直居中对齐的完整示例
- DirectWrite 渲染单行文字与测量文字包围盒（对比 GDI 的差异）

**常见坑**
- `DrawText` 的 `DT_CALCRECT` 与实际绘制尺寸在高 DPI 下不一致
- 混用 GDI 字体句柄与 DirectWrite 字体格式

**小作业**
- 实现一个「字体预览器」：选择字体 + 字号，实时展示一段示例文字，标注 baseline、ascent、descent 辅助线

---

### 2.6 资源管理模式

**概念讲解**
- Win32 资源类型：`RT_BITMAP`、`RT_ICON`、`RT_DIALOG`、`RT_STRING`、`RT_MANIFEST`
- RAII 封装 GDI 对象：`SelectObject` 的「保存-使用-恢复」三步模式
- 跨平台资源抽象：Qt `QResource`（`:/` 路径）vs 平台资源的映射策略

**代码结构**
```cpp
// RAII GDI 对象选择器
class ScopedSelectObject {
    HDC hdc_; HGDIOBJ old_;
public:
    ScopedSelectObject(HDC hdc, HGDIOBJ obj)
        : hdc_(hdc), old_(SelectObject(hdc, obj)) {}
    ~ScopedSelectObject() { SelectObject(hdc_, old_); }
};
```

**常见坑**
- `SelectObject` 后忘记恢复原对象 → GDI 对象泄漏
- `LoadBitmap` vs `LoadImage`：前者不支持高色深，现代代码应统一用 `LoadImage`

**小作业**
- 封装一个 `GdiBitmap` RAII 类，支持从资源/文件加载，自动管理句柄生命周期

---

## 第三篇 · 图形渲染（独立系列）

> 本篇从 GDI 出发，沿 Windows 图形栈演进脉络，逐步过渡到 Direct2D 和 Direct3D，形成**独立可订阅**的渲染系列。每一章都可以独立阅读，同时前后形成明确的知识依赖链。

```
GDI（CPU，兼容性强）
  ↓ 性能瓶颈 + 高 DPI 不友好
Direct2D / DirectWrite（GPU 加速 2D，Win7+）
  ↓ 需要 3D 效果 / 自定义渲染管线
Direct3D 11（完整可编程渲染管线入门）
  ↓ 需要更底层的 GPU 控制
Direct3D 12（现代显式 GPU 编程）
```

---

### 3.1 GDI 深入

#### 3.1.1 GDI 对象模型与生命周期

**概念讲解**
- GDI 对象的六种类型：Pen、Brush、Font、Bitmap、Region、Palette
- HDC（设备上下文）的本质：绘图状态机，而非画布本身
- `SelectObject` 的「换笔」语义：DC 同时只能持有一个同类对象

**代码结构**
- 完整的 GDI 对象使用模板：创建 → Select → 绘制 → 恢复 → 删除

**常见坑**
- 删除仍被 Select 的对象 → 未定义行为，GDI 内部状态损坏
- `CreateCompatibleDC` 后忘记 `DeleteDC`（vs `GetDC` 后需要 `ReleaseDC`，两套不同的释放路径）

**小作业**
- 用 GDI 绘制一个带阴影效果的文字标签（离屏 Bitmap + BitBlt 合成）

---

#### 3.1.2 区域（Region）与裁切

**概念讲解**
- Region 的三种形态：矩形、椭圆、多边形，以及布尔运算（`CombineRgn`）
- 裁切区域（Clip Region）vs 可视区域（Visible Region）的区别
- 用 Region 实现异形窗口（`SetWindowRgn`）

**代码结构**
- 圆角窗口的完整实现：`CreateRoundRectRgn` + `SetWindowRgn`

**常见坑**
- `SetWindowRgn` 后系统接管 Region 所有权，不能再手动 `DeleteObject`
- Region 坐标系是窗口客户区坐标，而非屏幕坐标

**小作业**
- 实现一个星形异形窗口（多边形 Region），支持鼠标拖动

---

#### 3.1.3 Alpha 混合与透明效果

**概念讲解**
- `AlphaBlend`（需要 `msimg32.lib`）：逐像素 Alpha 混合
- `TransparentBlt`：颜色键（Color Key）透明
- 分层窗口（Layered Window）：`WS_EX_LAYERED` + `UpdateLayeredWindow` 实现逐像素透明窗口
- `SetLayeredWindowAttributes`：简单的整窗口透明度 vs `UpdateLayeredWindow`：完整控制

**代码结构**
- 分层窗口绘制模板：创建 DIB Section → GDI 绘制到内存 DC → `UpdateLayeredWindow` 提交

**常见坑**
- `UpdateLayeredWindow` 的 `BLENDFUNCTION.SourceConstantAlpha` 与像素 Alpha 通道的叠加计算
- 分层窗口不接收 `WM_PAINT`，必须通过 `UpdateLayeredWindow` 主动更新

**小作业**
- 实现一个半透明的「悬浮球」窗口（圆形异形 + Alpha 透明），鼠标悬停时渐变变亮

---

### 3.2 GDI+ 入门

#### 3.2.1 GDI+ 与 GDI 的架构差异

**概念讲解**
- GDI+ 的设计目标：抗锯齿、渐变、图像格式、坐标变换
- C++ 封装层：`Gdiplus::Graphics` 对象 vs GDI 的 HDC
- 初始化：`GdiplusStartup` / `GdiplusShutdown` 的正确位置

**代码结构**
```cpp
Gdiplus::GdiplusStartupInput input;
ULONG_PTR token;
Gdiplus::GdiplusStartup(&token, &input, nullptr);
// ... 程序主体
Gdiplus::GdiplusShutdown(token);
```

**常见坑**
- 在 `WinMain` 结束后才调用 `GdiplusShutdown`，而全局 GDI+ 对象已析构 → 顺序问题

**小作业**
- 用 GDI+ 绘制一个带渐变填充的饼图，对比 GDI 实现同效果的代码量

---

#### 3.2.2 坐标变换与矩阵

**概念讲解**
- 2D 变换矩阵：平移、旋转、缩放、错切的矩阵表示
- `Graphics::SetTransform`：叠加变换 vs 替换变换
- 世界坐标 → 页面坐标 → 设备坐标的三级坐标系

**代码结构**
- 用矩阵变换实现「指针式仪表盘」的刻度绘制（旋转 + 绘制 + 恢复）

**常见坑**
- `Matrix::Multiply` 的乘法顺序：矩阵乘法不满足交换律，变换顺序影响结果

**小作业**
- 实现一个可旋转的图片查看器：鼠标拖动旋转，滚轮缩放，变换用矩阵维护

---

#### 3.2.3 图像格式与编解码

**概念讲解**
- GDI+ 内置支持：BMP、PNG、JPEG、GIF、TIFF、WMF/EMF
- `Image::FromFile` / `Image::FromStream`：文件路径 vs 内存流加载
- 编码参数：JPEG 质量设置（`EncoderParameters`）

**代码结构**
- 图片格式转换工具的核心代码：加载任意格式 → 保存为 PNG

**常见坑**
- `Image::FromFile` 会锁定文件，无法在程序运行时删除/修改原文件；需用 `FromStream` + 内存拷贝解锁

**小作业**
- 实现批量图片压缩工具：拖入多张图片，统一压缩为指定质量的 JPEG

---

### 3.3 Direct2D 与 DirectWrite

#### 3.3.1 Direct2D 架构与资源体系

**概念讲解**
- Direct2D 的定位：GDI 的现代替代，GPU 加速，与 Direct3D 互操作
- 资源两级分类：**设备无关资源**（`ID2D1Factory`、几何体、路径）vs **设备相关资源**（`ID2D1RenderTarget`、Brush、Bitmap）
- 渲染目标（Render Target）类型：`HwndRenderTarget`、`DCRenderTarget`、`BitmapRenderTarget`（离屏）、`DxgiSurfaceRenderTarget`（与 D3D 互操作）

**代码结构**
```cpp
// 标准 Direct2D 初始化骨架
ID2D1Factory* pFactory;
D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);

ID2D1HwndRenderTarget* pRT;
pFactory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),
    D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(width, height)),
    &pRT);
```

**常见坑**
- 设备相关资源在设备丢失（`D2DERR_RECREATE_TARGET`）时必须全部重建，不能复用
- 在 `BeginDraw` / `EndDraw` 外调用绘制方法 → 静默失败或崩溃

**小作业**
- 用 Direct2D 重写 3.1.1 的 GDI 绘图示例，对比代码结构与视觉质量

---

#### 3.3.2 几何体（Geometry）系统

**概念讲解**
- 内置几何体：矩形、圆角矩形、椭圆
- `ID2D1PathGeometry`：贝塞尔曲线、折线、弧线的自由路径
- 几何体操作：`CombineWithGeometry`（布尔运算）、`Widen`（描边扩展）、`Tessellate`（三角剖分）
- 几何体命中测试：`FillContainsPoint` / `StrokeContainsPoint`

**代码结构**
- `PathGeometry` 绘制星形的完整代码（`BeginFigure` → `AddLines` → `EndFigure`）

**常见坑**
- `Open` 的 `GeometrySink` 必须 `Close()` 才能使用几何体；未 Close 的几何体绘制时静默忽略

**小作业**
- 实现矢量图形编辑器原型：支持添加矩形/椭圆，鼠标拖动移动，命中测试选中

---

#### 3.3.3 效果（Effects）与图层

**概念讲解**
- `ID2D1Effect`（Win8+ Direct2D 1.1）：内置效果库（高斯模糊、阴影、色彩矩阵、形态学等）
- 效果图（Effect Graph）：多个效果链式连接
- `ID2D1Layer`：透明度图层、几何裁切图层（`PushLayer` / `PopLayer`）

**代码结构**
- 高斯模糊效果应用到 Bitmap 的示例
- 阴影效果（Shadow Effect）为图形添加投影的示例

**常见坑**
- `ID2D1Effect` 需要 Direct2D 1.1 设备上下文（`ID2D1DeviceContext`），而非普通 `HwndRenderTarget`

**小作业**
- 实现「毛玻璃卡片」效果：卡片后方背景模糊（高斯模糊 + 透明度图层合成）

---

#### 3.3.4 DirectWrite 高质量文字排版

**概念讲解**
- DirectWrite 的三层 API：文字格式（`IDWriteTextFormat`）→ 文字布局（`IDWriteTextLayout`）→ 渲染器（`IDWriteTextRenderer`）
- `IDWriteTextLayout` 的能力：精确命中测试、行/字符测量、内联对象（图文混排）
- DirectWrite 与 Direct2D 的集成：`RenderTarget::DrawTextLayout`

**代码结构**
```cpp
IDWriteTextFormat* pFormat;
pDWriteFactory->CreateTextFormat(
    L"Microsoft YaHei", nullptr,
    DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"zh-CN", &pFormat);
```
- 富文本渲染：同一 `TextLayout` 内不同文字段设置不同颜色/字号

**常见坑**
- `IDWriteFactory` 类型：`DWRITE_FACTORY_TYPE_SHARED` vs `ISOLATED`，前者复用缓存，生产代码首选
- 中文字体 Fallback：显式设置字体回退链避免方块字

**小作业**
- 实现代码高亮渲染器：用 `IDWriteTextLayout` 对关键字、字符串、注释分段着色

---

#### 3.3.5 Direct2D 与 Win32 / GDI 互操作

**概念讲解**
- `DCRenderTarget`：在 GDI 的 HDC 上使用 Direct2D（兼容旧代码渐进迁移路径）
- `ID2D1GdiInteropRenderTarget`：在 Direct2D 渲染目标上临时获取 HDC
- GDI+ 与 Direct2D 混用场景（打印、截图、兼容层）

**代码结构**
- 渐进迁移示例：现有 WM_PAINT 中 GDI 代码，用 `DCRenderTarget` 包装，逐步替换为 D2D 调用

**常见坑**
- GDI 与 Direct2D 在同一 DC 上交替绘制时，必须用 `Flush()` 同步 GPU 命令

**小作业**
- 将 3.1.3 的分层窗口示例迁移为 Direct2D 绘制（`BitmapRenderTarget` → `UpdateLayeredWindow`）

---

### 3.4 HLSL Shader 编程

#### 3.4.1 HLSL 语言基础

**概念讲解**
- HLSL 数据类型：标量、向量（`float4`）、矩阵（`float4x4`）、纹理（`Texture2D`）、采样器（`SamplerState`）
- 内置函数速查：数学函数（`dot`、`cross`、`normalize`、`lerp`）、纹理采样（`Sample`、`SampleLevel`）
- 语义（Semantic）：`POSITION`、`TEXCOORD`、`SV_Position`、`SV_Target` 的含义与用途

**代码结构**
```hlsl
// 最简 Vertex Shader
struct VSInput  { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct PSInput  { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

PSInput VS_Main(VSInput input) {
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), g_WorldViewProj);
    output.uv  = input.uv;
    return output;
}

// 最简 Pixel Shader
float4 PS_Main(PSInput input) : SV_TARGET {
    return g_Texture.Sample(g_Sampler, input.uv);
}
```

**常见坑**
- 向量分量顺序：HLSL `float4(r,g,b,a)` vs GLSL `vec4(x,y,z,w)`，跨 API 移植时颜色通道混淆
- `float` 精度：Shader 中隐式 `float` 均为 32 位，不存在 `double`（除非显式声明）

**小作业**
- 手写一个灰度化 Pixel Shader（对输入纹理颜色做灰度转换），在下一节的 D3D11 框架中运行

---

#### 3.4.2 HLSL 编译与调试

**概念讲解**
- 编译路径对比：运行时编译（`D3DCompileFromFile`）vs 离线编译（`fxc.exe` / `dxc.exe`）→ 嵌入字节码
- `dxc.exe`（DXC）：DXIL 字节码，支持 SM 6.x；`fxc.exe`：DXBC 字节码，SM 5.x 及以下
- Shader Model 版本速查：D3D11 → SM 5.0；D3D12 → SM 6.0+

**代码结构**
```cpp
// 运行时编译示例
ID3DBlob* pVSBlob;
ID3DBlob* pErrorBlob;
HRESULT hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr,
    "VS_Main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &pVSBlob, &pErrorBlob);
if (FAILED(hr)) {
    // 输出 pErrorBlob 中的错误信息
}
```
- CMake 集成 `fxc` 离线编译，将 `.cso` 文件作为资源嵌入可执行文件

**调试工具**
- PIX for Windows：GPU 帧捕获，查看每个 Draw Call 的 Shader 输入输出
- RenderDoc：跨平台 GPU 调试器，支持 D3D11/D3D12/Vulkan
- Visual Studio Graphics Debugger：集成在 VS 内，入门首选

**常见坑**
- 开发时用 `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`，发布时去掉 Debug 标志
- Shader 编译错误信息在 `pErrorBlob` 中，忽略它只看 `HRESULT` 无法定位问题

**小作业**
- 为项目配置 CMake 离线 Shader 编译流程，将 `.hlsl` 编译为 `.cso` 并自动拷贝到输出目录

---

#### 3.4.3 Constant Buffer 与数据传递

**概念讲解**
- Constant Buffer（`cbuffer`）：从 CPU 向 Shader 传递矩阵、参数的标准通道
- 内存对齐规则：HLSL `cbuffer` 的 16 字节对齐约束（packoffset / padding）
- 动态更新：`Map` / `Unmap` vs `UpdateSubresource` 的选择依据（频繁更新 vs 偶发更新）

**代码结构**
```hlsl
cbuffer PerFrameBuffer : register(b0) {
    matrix g_WorldViewProj;
    float4 g_Color;
    float  g_Time;
    float3 g_Padding; // 补齐到 16 字节
};
```

**常见坑**
- `float3` 后接 `float` → 实际占用 16 字节（float3 = 12 字节 + 4 字节 padding），C++ 结构体必须匹配
- 每帧 `Map`/`Unmap` `D3D11_MAP_WRITE_DISCARD` 是正确用法，`D3D11_MAP_WRITE` 需要 GPU 完成读取后才能写

**小作业**
- 通过 Constant Buffer 传递时间变量，实现 Shader 中的颜色随时间变化动画

---

### 3.5 Direct3D 11 渲染管线

#### 3.5.1 D3D11 初始化与 SwapChain

**概念讲解**
- 核心对象三件套：`ID3D11Device`（资源创建）、`ID3D11DeviceContext`（命令提交）、`IDXGISwapChain`（帧呈现）
- DXGI 层：硬件枚举、输出管理、SwapChain 管理，与 D3D 解耦
- SwapChain 参数：缓冲数量（双缓冲/三缓冲）、格式（`DXGI_FORMAT_R8G8B8A8_UNORM`）、翻转模式（`DXGI_SWAP_EFFECT_FLIP_DISCARD`）

**代码结构**
- D3D11 最小初始化：`D3D11CreateDeviceAndSwapChain` 的完整参数说明
- Resize 处理：`WM_SIZE` 时释放 RTV → `ResizeBuffers` → 重建 RTV 的标准流程

**常见坑**
- 在持有 `ID3D11RenderTargetView` 的情况下调用 `ResizeBuffers` → `E_INVALIDARG` 错误
- 忘记设置 `DXGI_SWAP_CHAIN_DESC.Windowed = TRUE` 导致全屏初始化失败

**小作业**
- 搭建 D3D11 框架：清屏为渐变颜色（通过 Constant Buffer 传递颜色）并呈现

---

#### 3.5.2 顶点缓冲与输入布局

**概念讲解**
- GPU 内存资源：`ID3D11Buffer`（顶点缓冲、索引缓冲、Constant Buffer）
- 输入布局（`ID3D11InputLayout`）：告知 D3D11 顶点结构体中每个字段的语义和格式
- 图元拓扑（Primitive Topology）：`TriangleList`、`TriangleStrip`、`LineList`、`PointList`

**代码结构**
```cpp
// 顶点结构体与输入布局定义
struct Vertex { XMFLOAT3 pos; XMFLOAT2 uv; };
D3D11_INPUT_ELEMENT_DESC layout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, ...},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, ...},
};
```

**常见坑**
- 输入布局必须与绑定的 Vertex Shader 字节码匹配，换 Shader 时必须重建或验证布局
- `AlignedByteOffset` 使用 `D3D11_APPEND_ALIGNED_ELEMENT` 让驱动自动计算偏移

**小作业**
- 渲染一个彩色三角形（每个顶点携带不同颜色，PS 内插值）

---

#### 3.5.3 纹理与采样器

**概念讲解**
- 纹理资源：`ID3D11Texture2D`、Mipmap 链、纹理数组
- 着色器资源视图（SRV）：将纹理暴露给 Shader 的视图层
- 采样器状态（`ID3D11SamplerState`）：过滤模式（Point/Linear/Anisotropic）、寻址模式（Wrap/Clamp/Mirror/Border）

**代码结构**
- 从 WIC（Windows Imaging Component）加载 PNG/JPEG 到 D3D11 纹理的完整工具函数
- 各过滤模式在缩放场景下的视觉对比示例

**常见坑**
- 忘记生成 Mipmap（`GenerateMips`）→ 纹理缩小时出现摩尔纹
- WIC 解码出的像素格式不一定是 `DXGI_FORMAT_R8G8B8A8_UNORM`，需要格式转换

**小作业**
- 渲染一个贴图正方形，实现可切换的过滤模式（按键切换 Point/Linear/Anisotropic）

---

#### 3.5.4 深度缓冲与 3D 变换

**概念讲解**
- 深度缓冲（Depth Buffer / Z-Buffer）：解决遮挡问题，`DXGI_FORMAT_D24_UNORM_S8_UINT`
- MVP 变换矩阵：Model（物体位置）× View（摄像机）× Projection（透视投影）
- `DirectXMath`：`XMMatrixLookAtLH`、`XMMatrixPerspectiveFovLH`、`XMMatrixRotationY`

**代码结构**
- Constant Buffer 传递 MVP 矩阵，Vertex Shader 中变换顶点位置的完整示例
- 摄像机类封装：位置、朝向、FOV，计算 View-Projection 矩阵

**常见坑**
- DirectXMath 是行向量约定（vs GLSL 的列向量），矩阵传给 Shader 前需要 `XMMatrixTranspose`
- 深度缓冲必须与渲染目标尺寸一致，`ResizeBuffers` 时同步重建

**小作业**
- 渲染一个旋转的 3D 立方体，每面不同颜色，摄像机可以用鼠标拖拽旋转（Arcball 摄像机）

---

#### 3.5.5 光照模型基础

**概念讲解**
- Phong 光照模型：环境光（Ambient）+ 漫反射（Diffuse）+ 镜面高光（Specular）
- 法向量（Normal）的变换：不能直接用 ModelMatrix，需要法线矩阵（逆转置矩阵）
- 光照计算在 Vertex Shader（Gouraud shading）vs Pixel Shader（Phong shading）的对比

**代码结构**
```hlsl
// Pixel Shader 中的 Phong 光照
float3 normal  = normalize(input.normal);
float3 lightDir = normalize(g_LightPos - input.worldPos);
float  diff    = max(dot(normal, lightDir), 0.0f);
float3 diffuse = diff * g_LightColor * g_MaterialColor;
```

**常见坑**
- 法向量在非均匀缩放的 Model 矩阵下变形，必须传法线矩阵而非 Model 矩阵
- 镜面高光计算需要摄像机位置（在 Constant Buffer 中传入），不能假设摄像机在原点

**小作业**
- 为 3.5.4 的旋转立方体添加 Phong 光照，实现可移动的点光源

---

#### 3.5.6 混合（Blend）与透明渲染

**概念讲解**
- `ID3D11BlendState`：Alpha 混合方程 `src_color * src_alpha + dst_color * (1 - src_alpha)`
- 透明物体渲染顺序问题：必须从远到近（Back-to-Front）绘制
- 预乘 Alpha（Premultiplied Alpha）vs 直通 Alpha（Straight Alpha）

**代码结构**
- 标准透明混合状态创建配置
- 透明粒子的排序 + 渲染流程

**常见坑**
- 深度写入（Depth Write）在透明渲染时需要禁用，否则后绘制的不透明物体被错误遮挡
- 渲染 UI 叠加层时忘记关闭深度测试，导致 UI 被 3D 物体遮盖

**小作业**
- 实现一个半透明粒子系统（100 个随机粒子，带透明度，从远到近排序渲染）

---

#### 3.5.7 阶段小项目：D3D11 2D 精灵渲染器

**功能规格**
- 支持贴图精灵（Sprite）的渲染：位置、旋转、缩放、颜色叠加、Alpha 透明
- 批处理合并（Sprite Batch）：同一纹理的多个精灵合并为一次 Draw Call
- 精灵图集（Texture Atlas / Sprite Sheet）支持：UV 子区域采样
- 文字渲染集成：用 DirectWrite 离屏渲染文字到纹理，再由精灵系统绘制

**目标**：能够驱动一个简单的 2D 游戏场景或 GUI 自绘框架原型

---

### 3.6 Direct3D 12 进阶

> 本章在 D3D11 基础上讲解 D3D12，重点讲**差异与新概念**，而非完整重复渲染管线基础。

#### 3.6.1 D3D12 设计哲学：显式控制

**概念讲解**
- D3D12 的核心目标：降低 CPU 驱动开销，让开发者显式管理 GPU 工作
- 与 D3D11 的关键差异对比表：

| 概念 | D3D11 | D3D12 |
|------|-------|-------|
| 命令提交 | 立即/延迟上下文 | CommandList + CommandQueue |
| 资源状态 | 驱动自动管理 | 手动 Resource Barrier |
| 内存管理 | 驱动分配 | 显式 Heap 分配 |
| 描述符 | 隐式绑定 | Descriptor Heap + Table |
| 同步 | 自动 | Fence 手动同步 |

**常见坑**
- D3D12 的学习曲线不在「渲染管线概念」，而在「同步与资源状态管理」

**小作业**
- 阅读微软 D3D12HelloWorld 示例代码，对照上表找出每个概念的对应代码位置

---

#### 3.6.2 命令列表、队列与围栏

**概念讲解**
- `ID3D12CommandAllocator`：命令内存池（不可复用，除非 GPU 完成）
- `ID3D12GraphicsCommandList`：录制命令，`Close()` 后提交
- `ID3D12CommandQueue`：`ExecuteCommandLists` 提交到 GPU
- `ID3D12Fence` + `HANDLE`（Event）：CPU 等待 GPU 完成的同步原语

**代码结构**
```cpp
// 帧同步标准模式
commandList->Close();
ID3D12CommandList* lists[] = { commandList };
commandQueue->ExecuteCommandLists(1, lists);
commandQueue->Signal(fence, ++fenceValue);
if (fence->GetCompletedValue() < fenceValue) {
    fence->SetEventOnCompletion(fenceValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
}
```

**常见坑**
- `CommandAllocator` 在 GPU 未完成时调用 `Reset` → 验证层报错甚至崩溃
- 多帧并行时（帧缓冲 N=2/3）每帧需要独立的 Allocator，不能共用

**小作业**
- 用 D3D12 渲染一个清屏动画（颜色随时间变化），正确处理帧同步

---

#### 3.6.3 资源与堆管理

**概念讲解**
- D3D12 内存堆类型：`DEFAULT`（GPU 本地）、`UPLOAD`（CPU 写 GPU 读）、`READBACK`（GPU 写 CPU 读）
- 创建方式三种：`CreateCommittedResource`（隐式堆，简单）、`CreatePlacedResource`（显式堆，灵活）、`CreateReservedResource`（虚拟内存，Tiled Resources）
- 资源状态（Resource State）与屏障（Resource Barrier）：`D3D12_RESOURCE_BARRIER_TYPE_TRANSITION`

**代码结构**
- 上传顶点数据的标准流程：CPU → Upload Buffer → `CopyBufferRegion` → Default Buffer → Barrier(COPY_DEST → VERTEX)

**常见坑**
- 忘记在上传后转换资源状态（`COPY_DEST → VERTEX_AND_CONSTANT_BUFFER`）→ 验证层错误
- Upload Buffer 在命令执行期间被释放 → GPU 读取到无效数据

**小作业**
- 封装 `GpuUploadBuffer` 工具类，用于将任意 CPU 数据上传到 GPU Default Buffer

---

#### 3.6.4 描述符堆与根签名

**概念讲解**
- 描述符堆（Descriptor Heap）：SRV/UAV/CBV 描述符的 GPU 可见数组
- 根签名（Root Signature）：定义 Shader 能看到哪些资源（Root Constants、Root Descriptor、Descriptor Table）
- 根签名序列化：HLSL 中嵌入 `RootSignature` 属性 vs C++ 代码创建

**代码结构**
- 简单根签名：一个 CBV + 一个 SRV + 一个 Sampler 的 C++ 创建代码

**常见坑**
- Descriptor Table 指向的 Descriptor Heap 范围越界 → GPU 读取随机内存
- 根签名版本（1.0 vs 1.1）：1.1 支持描述符静态标志优化，但兼容性需要检查

**小作业**
- 用 D3D12 渲染一个带纹理的矩形，配置完整的根签名和描述符堆

---

#### 3.6.5 D3D12 与 D3D11 互操作及选型建议

**概念讲解**
- `ID3D11On12Device`：在 D3D12 CommandQueue 上使用 D3D11 接口（迁移过渡方案）
- Direct2D on D3D12：通过 D3D11on12 + D2D `DCRenderTarget` 在 D3D12 帧中绘制 2D UI
- 实际项目选型建议：应用/工具类 → D3D11 足够；引擎/高性能渲染 → D3D12；移植 / 学习 → D3D11 先行

**小作业**
- 调研并总结：现有开源项目（如 ImGui）如何同时支持 D3D11 和 D3D12 后端（阅读源码，文字说明）

---

### 3.7 自定义控件绘制原理

#### 3.7.1 Owner-Draw 控件

**概念讲解**
- `WM_DRAWITEM`：系统在需要绘制时通知父窗口，传递 `DRAWITEMSTRUCT`
- 适用控件：`ListBox`、`ComboBox`、`Button`、`Menu`（设置 `BS_OWNERDRAW` / `LBS_OWNERDRAWFIXED` 等）
- `DRAWITEMSTRUCT`：`itemID`、`itemState`（选中/焦点/禁用）、`hDC`、`rcItem`

**代码结构**
- Owner-Draw ListBox 实现图标+文字列表项的完整示例

**常见坑**
- 未处理 `WM_MEASUREITEM` → Owner-Draw ListBox 项高度为 0，内容不可见
- `itemState & ODS_SELECTED` 判断时忘记绘制高亮背景，选中状态下文字看不见

**小作业**
- 实现一个 Owner-Draw ComboBox，下拉项显示颜色块 + 颜色名称

---

#### 3.7.2 完全自绘控件架构

**概念讲解**
- 自绘控件的四要素：状态机（Normal/Hover/Pressed/Disabled/Focused）、命中测试、键盘导航、无障碍接口
- 处理 `WM_PAINT`（背景）、`WM_LBUTTONDOWN/UP`（交互）、`WM_MOUSEMOVE` + `TrackMouseEvent`（Hover 状态）
- 焦点处理：`WM_SETFOCUS` / `WM_KILLFOCUS`，`WM_KEYDOWN` 响应 Space/Enter

**代码结构**
- 自绘 Button 类的完整框架（状态机 + Direct2D 绘制 + 消息处理）

**常见坑**
- 鼠标移出控件时 `WM_MOUSELEAVE` 不会自动发送，必须先调用 `TrackMouseEvent` 注册
- 在 `WM_PAINT` 中用 `GetCursorPos` + `ScreenToClient` 检测 Hover，而非维护状态 → 状态不一致

**小作业**
- 实现一个带动画的自绘开关（Toggle）控件：滑块从左到右平滑过渡

---

#### 3.7.3 命中测试与鼠标事件路由

**概念讲解**
- 矩形命中测试（`PtInRect`）vs 精确命中测试（`ID2D1Geometry::FillContainsPoint`）
- 鼠标捕获（`SetCapture` / `ReleaseCapture`）：拖拽场景中防止鼠标移出控件范围时丢失事件
- Z 序与事件穿透：`WS_EX_TRANSPARENT` 让鼠标事件穿透到下方窗口

**代码结构**
- 拖拽选择框（Rubber Band Selection）的完整实现

**常见坑**
- 鼠标按下后移动过快，`WM_MOUSEMOVE` 采样不足，直线段有跳跃 → 需要插值
- 忘记 `ReleaseCapture` → 其他窗口无法接收鼠标事件（系统级影响）

**小作业**
- 在绘图板上实现矩形选框工具（按住拖拽显示虚线选框，释放后保留选区）

---

### 3.8 OpenGL / Vulkan 与 GUI 集成

#### 3.8.1 在 Win32 窗口中嵌入 OpenGL

**概念讲解**
- WGL（Windows GL）：Win32 扩展 OpenGL 的胶合层，`wglCreateContext`、`wglMakeCurrent`
- Pixel Format 选择：`PIXELFORMATDESCRIPTOR`，颜色深度、深度缓冲、双缓冲设置
- 现代 OpenGL 上下文创建：`WGL_ARB_create_context` 扩展指定版本（3.3/4.6 Core Profile）

**代码结构**
- Win32 + OpenGL 3.3 最小初始化代码（含扩展加载，推荐 GLAD/GLEW）

**常见坑**
- 旧式 `wglCreateContext` 只能创建兼容模式上下文（Legacy），必须用 `wglCreateContextAttribsARB` 创建 Core Profile

**小作业**
- Win32 窗口内运行一个旋转三角形（OpenGL 3.3 Core Profile + GLAD + VAO/VBO）

---

#### 3.8.2 Qt QOpenGLWidget 封装

**概念讲解**
- `QOpenGLWidget` 三个虚函数：`initializeGL`（初始化）、`resizeGL`（尺寸变化）、`paintGL`（绘制）
- `QOpenGLFunctions` 混入：跨平台 OpenGL 函数表
- Qt + ImGui 集成：将 Dear ImGui 嵌入 `QOpenGLWidget`（实用工具开发场景）

**代码结构**
- `QOpenGLWidget` 子类标准框架 + VAO/VBO/Shader 的 Qt 风格封装

**常见坑**
- 在 `paintGL` 以外的函数中调用 OpenGL 函数：需要先 `makeCurrent()`，结束后 `doneCurrent()`

**小作业**
- Qt 应用内嵌 OpenGL 视口，同时在 Qt Widgets 区域显示渲染参数控制面板

---

## 第四篇 · 跨平台框架

> 每节统一格式：概念讲解 → 代码结构 → 常见坑 → 小作业

---

### ── Qt 篇 ──

### 4.1 Qt 的世界观

**概念讲解**
- Qt 历史演进：Qt4 → Qt5 → Qt6（模块化拆分、CMake 优先、C++17 要求）
- Qt 构建系统：qmake（.pro 文件，遗留）vs CMake（现代首选）+ `find_package(Qt6)`
- QObject 三大特性：对象树（自动内存管理）、MOC（元对象编译器，信号槽基础）、属性系统

**代码结构**
```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
target_link_libraries(myapp PRIVATE Qt6::Widgets)
set_target_properties(myapp PROPERTIES WIN32_EXECUTABLE TRUE)
```

**常见坑**
- Qt6 中 `QString` 不再隐式从 `char*` 构造（需要 `QString::fromUtf8`）
- 忘记在 `CMakeLists.txt` 中启用 `CMAKE_AUTOMOC` → 信号槽代码编译报错

**小作业**
- 用 Qt6 + CMake 搭建工程，创建一个带菜单栏的主窗口，菜单项触发消息框

---

### 4.2 Qt 信号与槽

**概念讲解**
- 声明：`signals:` 区段（无需实现体）；`slots:` 区段（普通成员函数）
- 连接函数重载：字符串宏 `SIGNAL/SLOT` vs 函数指针 vs Lambda（各自的类型安全与性能差异）
- 连接类型：`Qt::DirectConnection`（同线程直接调用）、`Qt::QueuedConnection`（跨线程队列）、`Qt::BlockingQueuedConnection`（同步阻塞，慎用）

**代码结构**
```cpp
// 现代函数指针连接（编译期类型检查）
connect(slider, &QSlider::valueChanged,
        label,  &QLabel::setNum);

// Lambda 连接（捕获上下文）
connect(button, &QPushButton::clicked, this, [this]() {
    label->setText("Clicked!");
});
```

**常见坑**
- 函数指针连接时，信号/槽有重载版本 → 需要 `qOverload<int>(&QSpinBox::valueChanged)` 消歧义
- Lambda 连接默认没有第三个 `context` 参数，发送者销毁后 Lambda 仍可能触发 → 内存访问违规

**小作业**
- 实现「温度转换器」：两个 `QDoubleSpinBox` 双向绑定，修改任一方实时更新另一方

---

### 4.3 Qt Widgets 布局系统

**概念讲解**
- 布局类层次：`QHBoxLayout` / `QVBoxLayout` / `QGridLayout` / `QFormLayout` / `QStackedLayout`
- `QSizePolicy`：水平/垂直方向的伸缩策略（Fixed、Minimum、Expanding、Preferred 等）
- 伸缩因子（`setStretch`）：多控件分割可用空间的权重比

**代码结构**
- 「设置对话框」的嵌套布局实现：左侧列表 + 右侧内容区（`QSplitter` + `QStackedWidget`）

**常见坑**
- 直接 `setGeometry` 后再 `setLayout` → 布局接管几何管理，手动设置被覆盖
- `QWidget` 设置了固定大小策略但父布局有 `Expanding` 子控件 → 出现意外留白

**小作业**
- 实现响应式「仪表板」界面：窗口缩放时，三个信息卡片按比例均匀排列

---

### 4.4 Qt 常用控件全览

**概念讲解**
- 输入控件：`QLineEdit`（验证器 `QValidator`）、`QTextEdit`、`QSpinBox`/`QDoubleSpinBox`、`QComboBox`
- 展示控件：`QLabel`（支持富文本 HTML）、`QProgressBar`、`QLCDNumber`
- 容器控件：`QGroupBox`、`QTabWidget`、`QScrollArea`、`QSplitter`、`QDockWidget`
- 对话框：`QFileDialog`、`QColorDialog`、`QFontDialog`、`QMessageBox`、`QInputDialog`

**代码结构**
- `QLineEdit` + `QIntValidator` 实现只允许输入端口号的输入框

**常见坑**
- `QLabel::setText` 传入含 `<` 字符的用户内容 → 被当作 HTML 解析（需要 `Qt::PlainText` 或转义）
- `QComboBox::addItems` 与 `currentIndexChanged` 信号在初始化时的触发顺序问题

**小作业**
- 实现「颜色选择器」面板：RGB 三个滑块 + 十六进制输入框 + 颜色预览块，四者实时同步

---

### 4.5 Qt Model/View 架构

**概念讲解**
- 三角色分离：Model（数据）/ View（显示）/ Delegate（渲染+编辑）
- 标准模型：`QStringListModel`、`QStandardItemModel`、`QFileSystemModel`
- 自定义 Model：继承 `QAbstractItemModel` 必须实现的五个纯虚函数（`index`、`parent`、`rowCount`、`columnCount`、`data`）

**代码结构**
```cpp
// 最小自定义 List Model
class MyModel : public QAbstractListModel {
    int rowCount(const QModelIndex&) const override { return data_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (role == Qt::DisplayRole) return data_[idx.row()];
        return {};
    }
    std::vector<QString> data_;
};
```

**常见坑**
- 修改 Model 数据前忘记调用 `beginInsertRows/beginRemoveRows` → View 不刷新或崩溃
- 使用 `QStandardItemModel` 存储大量数据（万级行）→ 性能问题，应改用自定义 Model

**小作业**
- 实现「联系人管理」：自定义 Model 存储联系人（姓名/电话/邮件），`QTableView` 展示，支持排序和搜索过滤（`QSortFilterProxyModel`）

---

### 4.6 Qt Style Sheets（QSS）

**概念讲解**
- QSS 语法：选择器（类型、类名 `#id`、伪状态 `:hover/:checked/:disabled`）+ 属性
- 盒模型属性：`margin`、`padding`、`border`、`background`
- QSS 与 CSS 的核心差异：不支持继承、不支持动画（需要 `QPropertyAnimation`）、`subcontrol`（`::handle`、`::item`）

**代码结构**
```css
QPushButton {
    background: #0078d4; color: white;
    border-radius: 4px; padding: 6px 16px;
}
QPushButton:hover    { background: #106ebe; }
QPushButton:pressed  { background: #005a9e; }
QPushButton:disabled { background: #cccccc; color: #666; }
```

**常见坑**
- `QGroupBox` 在 QSS 中设置背景色后子控件背景变透明 → 需要同时设置子控件背景
- QSS 选择器优先级规则与 CSS 不同，多次 `setStyleSheet` 的覆盖顺序需要注意

**小作业**
- 用纯 QSS 实现一套「深色主题」，覆盖主窗口、菜单栏、工具栏、状态栏和常用控件

---

### 4.7 Qt 多线程

**概念讲解**
- `QThread` 的两种用法：子类化 + 重写 `run()`（不推荐） vs Worker Object + `moveToThread()`（推荐）
- Worker Object 模式：Worker 无 UI，通过信号槽与主线程通信
- `QThreadPool` + `QRunnable` / `QtConcurrent::run`：任务并发，无需手动管理线程生命周期

**代码结构**
```cpp
// Worker Object 模式（正确用法）
auto* worker = new DownloadWorker;
auto* thread = new QThread;
worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &DownloadWorker::doWork);
connect(worker, &DownloadWorker::finished, thread, &QThread::quit);
connect(thread, &QThread::finished, worker, &QObject::deleteLater);
thread->start();
```

**常见坑**
- 在 Worker 的槽函数中直接操作 UI 控件（`setText` 等）→ 跨线程操作 UI，未定义行为
- `QThread::terminate()` 强制终止 → 资源泄漏，几乎任何情况下都不应使用

**小作业**
- 实现带取消按钮的文件哈希计算器：Worker 线程计算，每处理 1MB 通过信号更新进度条，取消时正确停止线程

---

### 4.8 Qt 网络与文件

**概念讲解**
- 文件系统：`QFile`（读写）、`QDir`（目录遍历）、`QFileSystemWatcher`（文件变化监听）
- 序列化：`QDataStream`（二进制）、`QJsonDocument`（JSON）、`QXmlStreamReader/Writer`（XML）
- 网络：`QTcpSocket` / `QUdpSocket`（低层）、`QNetworkAccessManager`（HTTP 高层）

**代码结构**
```cpp
// HTTP GET 请求
auto* manager = new QNetworkAccessManager(this);
auto* reply = manager->get(QNetworkRequest(QUrl("https://example.com/api")));
connect(reply, &QNetworkReply::finished, this, [reply]() {
    if (reply->error() == QNetworkReply::NoError) {
        auto json = QJsonDocument::fromJson(reply->readAll());
    }
    reply->deleteLater();
});
```

**常见坑**
- `QFile` 在 Windows 上路径分隔符：`/` 和 `\\` 都支持，但避免与 `QDir::separator()` 混用
- `QNetworkReply` 必须手动 `deleteLater`，否则内存泄漏

**小作业**
- 实现「实时日志查看器」：`QFileSystemWatcher` 监听日志文件变化，`QPlainTextEdit` 追加显示新内容

---

> **阶段小项目**：用 Qt 重新实现「文本编辑器」（对比 Win32 版本，体会框架抽象层次）

---

### ── wxWidgets 篇 ──

### 4.9 wxWidgets 设计哲学

**概念讲解**
- 核心理念：使用原生控件，外观与系统原生应用完全一致
- 事件表（Event Table）机制：`BEGIN_EVENT_TABLE` 宏 vs 现代 `Bind` 方法
- `wxApp`、`wxFrame`、`wxPanel` 的层次关系；`wxApp::OnInit` 作为程序入口

**代码结构**
```cpp
class MyApp : public wxApp {
public:
    bool OnInit() override {
        auto* frame = new MyFrame("Hello wxWidgets");
        frame->Show();
        return true;
    }
};
wxIMPLEMENT_APP(MyApp);
```

**常见坑**
- 在非主线程中创建或操作 wxWidgets 控件 → 跨线程 UI 操作，平台行为不一致
- Windows 上需要 `wxMS_WINMAIN` 入口约定，CMake 中需设置 `WIN32` 可执行文件属性

**小作业**
- 搭建 wxWidgets CMake 工程，实现与 Qt 4.1 同等功能的带菜单主窗口

---

### 4.10 wxWidgets 核心控件与布局

**概念讲解**
- Sizer 系统：`wxBoxSizer`（水平/垂直）、`wxGridSizer`、`wxFlexGridSizer`、`wxStaticBoxSizer`
- `wxSizer::Add` 参数：proportion（伸缩比例）、flag（对齐/边框选项）、border（像素边距）
- XRC 资源文件：XML 描述界面，`wxXmlResource::LoadFrame` 加载，实现界面与逻辑分离

**代码结构**
- `wxFlexGridSizer` 实现表单布局（标签列固定宽度，输入列自动伸缩）的完整示例

**常见坑**
- `proportion=0` 的控件不参与空间分配，窗口缩放时大小不变，这是设计，不是 Bug
- XRC 文件中控件 `name` 属性必须唯一，`XRCCTRL` 宏通过名称查找

**小作业**
- 用 XRC 文件描述「偏好设置」对话框（三个标签页），C++ 代码只负责加载和事件处理

---

> **阶段小项目**：wxWidgets 跨平台「图片查看器」（支持 Windows/Linux/macOS 编译）

---

### ── GTK 篇 ──

### 4.11 GTK 生态与 C++ 绑定

**概念讲解**
- GTK4 vs GTK3：GTK4 的 Snapshot/Widget 渲染模型变化，`GtkApplication` 强制化
- 构建系统：Meson + `pkg-config` 查询 GTK 依赖（Linux 原生工具链）；Windows 用 vcpkg / MSYS2
- gtkmm：GTK 的 C++ 官方绑定，RAII 资源管理，类型安全的信号连接

**代码结构**
```cpp
// GTK4 + gtkmm 最小程序
class MyWindow : public Gtk::ApplicationWindow {
public:
    MyWindow() { set_title("Hello GTK"); set_default_size(400, 300); }
};
int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create("org.example.hello");
    return app->make_window_and_run<MyWindow>(argc, argv);
}
```

**常见坑**
- GTK4 移除了 `GtkContainer::add`（用 `child` 属性或专属 API 替代）
- Windows 上 GTK 应用字体渲染与 Linux 不同，需要额外配置字体后端

**小作业**
- GTK4/gtkmm 搭建工程，实现带工具栏和状态栏的主窗口

---

### 4.12 GTK 信号机制与 GObject

**概念讲解**
- GObject 类型系统：C 实现的面向对象基础，`G_OBJECT` 宏家族
- 信号连接：`g_signal_connect`（C API）vs gtkmm 的 `signal_xxx().connect`（C++ Lambda 友好）
- 属性（Properties）与通知：`g_object_set`/`g_object_get`，`notify::property-name` 信号

**代码结构**
- gtkmm 信号连接的三种方式：成员函数指针 / `sigc::ptr_fun` / Lambda

**常见坑**
- `g_signal_connect` 的回调函数签名必须完全匹配，参数数量/类型错误是未定义行为（C API 无类型检查）

**小作业**
- 用 GObject 属性绑定实现双向数据同步：Slider 与 SpinButton 共享同一属性值

---

### 4.13 GTK 布局容器

**概念讲解**
- GTK4 布局容器：`GtkBox`（线性）、`GtkGrid`（网格）、`GtkOverlay`（叠加层）、`GtkPaned`（可拖分割）
- CSS 主题系统：GTK 的 CSS 引擎，`GtkCssProvider` 加载自定义样式
- GTK Inspector：运行时调试 GTK 控件树和 CSS（`Ctrl+Shift+D`）

**代码结构**
- `GtkOverlay` 实现视频播放器浮动控制栏（控制条叠加在视频上方）

**常见坑**
- GTK CSS 与 Web CSS 差异较大（属性支持有限，选择器语法有子集限制）
- 高 DPI（HiDPI）下图标资源需要提供 `@2x` 版本或使用 SVG 格式

**小作业**
- 用 CSS provider 实现深色主题切换按钮（运行时动态加载/卸载主题 CSS）

---

> **阶段小项目**：GTK/gtkmm「系统信息面板」（Linux 原生风格，显示 CPU/内存/磁盘信息）

---

## 第五篇 · 现代 Windows 技术

### 5.1 WinUI 3 与 Windows App SDK

**概念讲解**
- 演进路径：Win32 → WPF / WinForms → UWP → WinUI 3（解耦 OS，支持桌面应用）
- Windows App SDK（WinAppSDK）：提供 WinUI 3、MRT Core、AppLifecycle、Push Notifications 等
- 两种部署方式：Framework-dependent（依赖系统预装 WinAppSDK）vs Self-contained（全量打包）

**代码结构**
- `App.xaml` + `MainWindow.xaml` 的最小 WinUI 3 工程结构（C++/WinRT 版本）
- `MainWindow.xaml` 中 XAML 与 `MainWindow.xaml.cpp` 中 C++ 的对应关系

**常见坑**
- WinUI 3 不支持 Windows 10 1809 以下系统（min target = 1809）
- C++/WinRT 的 `co_await` 异步代码需要正确处理线程切换（`co_await winrt::resume_foreground`）

**小作业**
- 创建 WinUI 3 工程，实现一个 Fluent Design 风格的设置页面（NavigationView + 内容区）

---

### 5.2 XAML 标记语言与数据绑定

**概念讲解**
- XAML 基础语法：元素属性、附加属性（`Grid.Row`）、内容属性、标记扩展（`{Binding}`、`{x:Bind}`）
- `{Binding}` vs `{x:Bind}`：前者运行时反射（类型不安全）；后者编译时生成代码（类型安全、性能更好，WinUI 3 推荐）
- `INotifyPropertyChanged`：属性变更通知接口，ViewModel 属性变化自动刷新 UI

**代码结构**
```xml
<!-- x:Bind 示例：编译时绑定 -->
<TextBlock Text="{x:Bind ViewModel.UserName, Mode=TwoWay}"/>
<ListView ItemsSource="{x:Bind ViewModel.Items}"/>
```

**常见坑**
- `{x:Bind}` 默认是 `OneTime` 模式（不是 `OneWay`），需要数据更新时须显式写 `Mode=OneWay`
- ViewModel 属性未正确触发 `PropertyChanged` → UI 不刷新，调试时现象是绑定无效

**小作业**
- 实现「待办事项」应用 MVVM 版本：`ObservableCollection<TodoItem>` 驱动 `ListView`，支持添加/删除/完成状态切换

---

### 5.3 WinRT API 的使用

**概念讲解**
- C++/WinRT 基础：`winrt::hstring`、`IAsyncOperation<T>`、`co_await`、`winrt::apartment_context`
- 常用 WinRT API 类别：文件选择器（`FileOpenPicker`）、系统通知（`ToastNotificationManager`）、剪贴板（`Clipboard`）、系统主题（`UISettings`）

**代码结构**
```cpp
// 异步文件选择器
auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
picker.FileTypeFilter().Append(L".txt");
auto file = co_await picker.PickSingleFileAsync();
if (file) { /* 处理文件 */ }
```

**常见坑**
- `FileOpenPicker` 等 UI 类需要在主线程（UI 线程）调用，且需要关联窗口句柄（WinUI 3 中用 `InitializeWithWindow`）
- `co_await` 后可能切换到线程池，访问 UI 控件前必须 `co_await winrt::resume_foreground(dispatcher)`

**小作业**
- 实现「截图工具」：使用 `Windows.Graphics.Capture` API 捕获屏幕区域，保存为 PNG 文件

---

### 5.4 打包为 MSIX

**概念讲解**
- MSIX 包结构：`AppxManifest.xml`（应用元数据）、`Assets/`（图标）、应用程序文件
- 声明式能力（Capabilities）：`filesystemRead`、`webcam`、`microphone` 等需要在 Manifest 中声明
- 打包方式：Visual Studio 打包向导 vs `MakeAppx.exe` 命令行 vs `winget-create`

**代码结构**
- 最小 `AppxManifest.xml` 示例：应用身份、入口点、视觉资源声明

**常见坑**
- MSIX 包中路径不能包含中文或特殊字符
- 调试时用「部署」而非「运行」安装 MSIX，否则文件关联等声明不生效

**小作业**
- 将 5.1 的 WinUI 3 应用打包为 MSIX，配置文件类型关联（双击 `.myapp` 文件启动程序）

---

## 第六篇 · Web 混合方案

### 6.1 为什么选择混合方案？

**概念讲解**
- 原生 UI vs Web UI 权衡矩阵：开发效率、外观定制、性能、包体积、跨平台性
- 混合方案适用场景：设置页面、帮助文档、数据可视化（D3.js/ECharts）、商业智能看板

**代码结构**
- 决策树：何时用原生控件，何时嵌入 Web 内容

**常见坑**
- Web 内容处理文件系统/系统 API 时需要通过 C++ 桥接，安全边界设计不当易产生漏洞

**小作业**
- 分析三款实际产品（VS Code / Notion / Figma）的 UI 架构，识别哪些部分是原生渲染，哪些是 Web 渲染

---

### 6.2 WebView2

**概念讲解**
- WebView2 架构：基于 Chromium，常青（Evergreen）运行时 vs 固定版本（Fixed Version）分发
- 初始化流程：`CreateCoreWebView2EnvironmentWithOptions` → `CreateCoreWebView2Controller` → 设置 Bounds
- 双向通信：C++ → JS（`ExecuteScript`）；JS → C++（`AddWebMessageReceivedEventHandler` + `window.chrome.webview.postMessage`）

**代码结构**
```cpp
// JS → C++ 消息接收
webview->add_WebMessageReceived(
    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
            wil::unique_cotaskmem_string message;
            args->TryGetWebMessageAsString(&message);
            // 处理 message.get()
            return S_OK;
        }).Get(), nullptr);
```

**常见坑**
- WebView2 初始化是异步的，`CreateCoreWebView2Controller` 完成前不能调用任何 WebView2 API
- 消息通信时 JSON 序列化/反序列化需要统一约定，避免版本不一致

**小作业**
- Win32 窗口内嵌 WebView2，实现 C++ 与 JavaScript 的双向通信（C++ 发送系统信息，JS 渲染图表）

---

### 6.3 CEF（Chromium Embedded Framework）

**概念讲解**
- CEF vs WebView2：CEF 自带 Chromium，包体大（~100MB）但版本可控；WebView2 依赖系统，包体小
- CEF 进程模型：主进程 + 渲染进程（每个 Tab 独立进程）+ GPU 进程
- `CefApp` / `CefClient` / `CefBrowserHost`：三个核心接口

**代码结构**
- CEF 最小集成示例：`CefInitialize` → `CefCreateBrowserSync` → `CefRunMessageLoop` → `CefShutdown`

**常见坑**
- CEF 需要在入口函数**最开始**处理子进程启动（`CefExecuteProcess`），否则子进程逻辑异常
- Windows 上 CEF 的 Visual C++ Runtime 版本与项目必须一致

**小作业**
- 用 CEF 实现一个内嵌浏览器，支持地址栏输入、前进/后退按钮

---

### 6.4 Tauri 简介（C++ 视角）

**概念讲解**
- Tauri 架构：Rust 后端 + 系统 WebView（Windows 用 WebView2 / macOS 用 WKWebView）
- 与 Electron 的差异：无捆绑 Chromium，包体极小（< 5MB），但不支持 C++ 后端
- 从 C++ 项目视角：可以通过 IPC 调用 Tauri sidecar 进程，或考虑 Tauri + C++ FFI（niche 用法）

**代码结构**
- Tauri 命令（`#[tauri::command]`）调用流程示意（Rust 侧），帮助 C++ 开发者理解类比关系

**常见坑**
- Tauri 不适合需要 C++ 深度集成的项目，更适合以 Web 前端为主、系统 API 为辅的工具类应用

**小作业**
- 调研对比：同一个「Markdown 编辑器」用 Electron vs Tauri vs Win32+WebView2 实现的包体大小、内存占用和启动速度

---

## 第七篇 · 工程化专题

### 7.1 多线程与 UI 线程安全

**概念讲解**
- GUI 黄金法则：所有 UI 操作必须在创建控件的线程（通常是主线程）执行
- 各框架线程调度方法：Win32 `PostMessage` / Qt `QMetaObject::invokeMethod` / GTK `g_idle_add` / WinUI `DispatcherQueue`
- 线程安全数据模型：`std::atomic`、`std::mutex` + 锁、无锁队列

**代码结构**
- 通用模式：任务线程 → 线程安全队列 → UI 线程定时轮询/事件唤醒 → 批量更新 UI

**常见坑**
- 在工作线程中持有 UI 控件的智能指针 → 控件销毁后析构在工作线程触发，引发跨线程析构
- `QMetaObject::invokeMethod` 忘记指定 `Qt::QueuedConnection` → 变成同步调用，潜在死锁

**小作业**
- 实现线程安全的「日志控制台」：多线程同时写入日志，UI 线程每 100ms 批量追加到 TextEdit

---

### 7.2 DPI 适配与高分辨率支持

**概念讲解**
- DPI 基本概念：96 DPI 基准，缩放因子（Scale Factor = DPI / 96），逻辑像素 vs 物理像素
- Win32 DPI 感知级别：Unaware → System → Per-Monitor → Per-Monitor V2
- Manifest 声明 DPI 感知：`dpiAware`、`dpiAwareness` 字段
- 各框架 DPI 处理：Win32 手动缩放坐标 / Qt 自动（`AA_EnableHighDpiScaling`）/ WinUI 3 原生支持

**代码结构**
- `WM_DPICHANGED` 处理模板：获取新 DPI → 缩放字体/控件 → `SetWindowPos` 调整窗口大小

**常见坑**
- 混合 DPI 感知级别：进程内同时存在 System Aware 和 Per-Monitor Aware 控件
- `GetSystemMetrics` 在 Per-Monitor DPI 下返回主显示器的值，需用 `GetSystemMetricsForDpi`

**小作业**
- 在双显示器（不同 DPI）环境下测试 Win32 程序：拖动窗口到不同屏幕时字体和控件是否正确缩放

---

### 7.3 自定义控件设计模式

**概念讲解**
- 控件通用接口设计：`Measure(availableSize) → Arrange(finalRect) → Render(painter)` 三段式
- 状态机设计：状态枚举 + 状态转换表 + 转换触发条件（输入/定时/外部）
- 动画系统接入：属性动画（插值计算）→ 定时器驱动 → 触发重绘

**代码结构**
- 通用属性动画类：`Animation<T>`，支持线性/缓入/缓出插值，回调通知

**常见坑**
- 动画帧率与 `WM_TIMER`/`QTimer` 精度不足（15ms 粒度）→ 使用 `QueryPerformanceCounter` 计算实际 delta time

**小作业**
- 实现「评分星标」控件：5 颗星，悬停时高亮预览，点击确认，带颜色过渡动画

---

### 7.4 打包与部署

**概念讲解**
- Windows 打包方案对比：

| 方案 | 适用场景 | 优点 | 缺点 |
|------|----------|------|------|
| Inno Setup | 传统桌面应用 | 脚本灵活 | 无沙箱 |
| NSIS | 老牌工具 | 社区大 | 脚本语法古旧 |
| MSIX | 商店 / 企业部署 | 干净卸载、自动更新 | 需证书签名 |
| 便携版（xcopy）| 开发者工具 | 零安装 | 无关联文件类型 |

- Linux：AppImage（单文件）/ Flatpak（沙箱）/ Snap（Ubuntu 生态）/ 发行版包（deb/rpm）
- macOS：`.app` Bundle 结构 / DMG 分发 / Notarization 公证流程

**代码结构**
- Inno Setup 脚本模板：安装路径、快捷方式、注册表写入、卸载清理

**常见坑**
- Visual C++ Runtime 依赖：静态链接（`/MT`）vs 依赖 VC Redist；Qt 部署用 `windeployqt`
- 代码签名证书过期导致 SmartScreen 拦截安装

**小作业**
- 为文本编辑器项目制作 Inno Setup 安装包，包含：快捷方式、`.txt` 文件关联、卸载支持

---

### 7.5 国际化与本地化

**概念讲解**
- 字符编码：UTF-8（源文件/存储）vs UTF-16（Win32 API 内部）vs 本地 ANSI（遗留兼容）
- Win32 本地化：`LoadString`（`RT_STRING` 资源）+ 多语言 DLL（Satellite DLL）
- Qt 翻译：`tr()` 标记 → `lupdate` 提取 → `Qt Linguist` 翻译 → `lrelease` 编译 `.qm` → `QTranslator` 加载

**代码结构**
- Qt 运行时切换语言（无需重启）的实现模式：`QTranslator::load` → `installTranslator` → 重建 UI

**常见坑**
- 硬编码中文字符串（而非通过 `tr()`）→ 翻译系统无法提取
- 日期/数字/货币格式的本地化：`QLocale` vs `GetLocaleInfo`

**小作业**
- 为文本编辑器添加中英双语支持，实现菜单栏语言切换（不重启）

---

### 7.6 辅助功能（Accessibility）

**概念讲解**
- 为什么要做无障碍：法规要求（美国 Section 508、欧盟 EN 301 549）+ 用户群体
- Windows UI Automation（UIA）：控件提供者（`IRawElementProviderSimple`）与客户端（辅助技术软件）
- ARIA（Web）与 Qt Accessibility（`QAccessible` 接口）的对应概念

**代码结构**
- 自绘控件实现 UIA 提供者接口的最小示例（使屏幕阅读器可以朗读控件内容）

**常见坑**
- 使用图片按钮未设置 Alt 文字 → 屏幕阅读器无法描述功能
- 自绘控件未实现 `WM_GETOBJECT` 消息处理 → 完全对辅助技术不可见

**小作业**
- 用 Windows 自带「讲述人」（Narrator）测试文本编辑器，记录哪些控件无法被识别并修复

---

### 7.7 性能优化

**概念讲解**
- 重绘性能优化：最小化脏区 `InvalidateRect`、避免 `WM_PAINT` 中的重量级计算、离屏缓冲
- 虚拟列表（Virtual List）：仅渲染可见行，`ListView` (`LVS_OWNERDATA`) / `QAbstractItemView::setUniformRowHeights`
- 启动速度优化：延迟初始化（Lazy Init）、资源按需加载、预编译头（PCH）

**代码结构**
- Win32 虚拟列表（`LVS_OWNERDATA`）实现百万行列表的完整示例
- 离屏双缓冲模板：`CreateCompatibleDC` + `CreateCompatibleBitmap` → 绘制 → `BitBlt` 到屏幕

**常见坑**
- 在 `WM_PAINT` 中每次重新加载图片资源 → 磁盘 I/O 成为绘制瓶颈
- 虚拟列表缓存策略不当 → 滚动时频繁重新计算，丢失流畅感

**小作业**
- 实现一个展示 100 万行数据的虚拟列表，滚动帧率不低于 60fps（`QueryPerformanceCounter` 测量）

---

## 第八篇 · 综合实战项目

### 项目：跨平台 Markdown 编辑器

**功能规格**
- 左侧编辑区 + 右侧实时预览（500ms 防抖刷新）
- 文件操作：新建、打开、保存、另存为、最近文件列表（MRU）
- 工具栏 + 菜单栏 + 状态栏（字数、行号、编码、光标位置）
- 语法高亮：Markdown 语法着色（标题、加粗、代码块、链接）
- 系统主题跟随：深色/浅色模式自动切换

**各框架实现版本**

| 框架 | 侧重展示的知识点 | 章节依赖 |
|------|----------------|----------|
| Win32 + WebView2 | 第一篇 + 第六篇综合；原生窗口 + Web 预览通信 | 1.x, 6.2 |
| Qt | Model/View、QSS 主题、多线程渲染、国际化 | 4.1–4.8, 7.1, 7.5 |
| wxWidgets | 跨平台构建、原生外观、XRC 界面分离 | 4.9–4.10 |
| GTK/gtkmm | Linux 原生集成、CSS 主题、GTK4 新特性 | 4.11–4.13 |
| WinUI 3 | XAML 数据绑定、Fluent Design、MSIX 打包 | 5.1–5.4 |

**阶段验收检查清单**
- [ ] 大文件（> 10MB Markdown）打开不卡顿（异步加载 + 虚拟列表）
- [ ] 多显示器 DPI 缩放正确
- [ ] 中文输入法（IME）字符输入无乱码
- [ ] 关闭未保存文档时弹出确认
- [ ] 安装包制作完成，可在干净系统上部署运行

---

> 一直在路上，持续更新中...