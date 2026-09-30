# 架构 · 从具象到抽象，再落回具象

> **原则**：架构不是起点，是 M1 走完才到达的地方。
> 教程的顺序是 **具象（裸 Win32 写一遍）→ 抽象（从痛点里长出封装）→ 落回具象（用框架重写同一件事）**。
> 抽象只能由痛点驱动 —— 每一层封装都必须先有"没有它之前的样子"。
>
> 相关：[positioning.md](positioning.md)（器官是身份，Win32 是遗产）· [win32-essentials.md](win32-essentials.md)（Win32 穿插清单）· [README.md](README.md)（规划索引）· [../Roadmap.md](../Roadmap.md)

---

## 〇、三段路线

| 段 | 读者在干什么 | 产出 | 验收 |
|:--|:--|:--|:---|
| **M1a** | 裸 Win32 直接写，不用任何框架 | 一个能跑的程序（单文件） | 七条"能用"判据，全过 |
| **M1b** | 回头看 M1a 的代码，把痛点逐个拔出来，长出五层封装 | 一具骨架 + "痛点 → 封装"对照 | 每一步都能指出它解决了 M1a 的哪个痛点 |
| **M1c** | 用框架把同一个 demo 重写 | 框架版 demo | **与 M1a 行为完全一致，代码显著更短** |

M1c 那条验收比七条硬得多：它同时验证了抽象的**正确性**（行为对得上）和**必要性**（代码确实短了）。

### M1a 的七条"能用"判据

1. 开一个窗口，里面有按钮、文本、输入框，能点、能输入
2. 缩放窗口时内容按布局重排，不闪烁
3. 中文能显示
4. 高 DPI 下不糊、不错位
5. 改一个状态 → 界面更新
6. 关掉时干净退出，无泄漏
7. 一份 CMake，一条命令构建

七条全过 = 初步能用。任何一条不过，那是"能演示"，不是"能用"。

---

## 一、M1a · 具象：裸 Win32 版

**形态**：一个文件，`app/m1a_naked.cpp`，约 250 行。**不许有 `class`，不许有封装。** 这一段的全部价值，就是让读者看清 OS 的每一根线是怎么接的。

**代码顺序 = 讲述顺序**：

```
wWinMain
 ├─ SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)      ← 判据第 4 条
 ├─ InitCommonControlsEx(...)                                ← 用控件的前提
 ├─ RegisterClassExW(...)                                    ← 注册窗口类
 ├─ CreateWindowExW(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN)   ← 主窗口
 │    └─ WM_CREATE 里建三个子控件（STATIC / EDIT / BUTTON）
 │         并 SendMessage(WM_SETFONT) 设字体                 ← 不设，中文又小又丑
 ├─ 消息循环 GetMessage → TranslateMessage → DispatchMessage
 └─ WndProc
      ├─ WM_COMMAND    → 按钮点击：GetWindowText → SetWindowText → InvalidateRect
      ├─ WM_SIZE       → 手算坐标，SetWindowPos 摆三个控件
      ├─ WM_DPICHANGED → SetWindowPos 照建议矩形走
      ├─ WM_PAINT      → BeginPaint / EndPaint
      └─ WM_DESTROY    → PostQuitMessage
```

**demo 内容**（小到一眼看完，但五条痛点全撞得上）：

> 一个窗口、一个输入框、一个按钮、一行文本。点按钮，把输入框里的字搬到文本上。

**穿插知识**：从 [win32-essentials.md](win32-essentials.md) 取 A–F 六组，就地讲一两句，不展开。

### ⚠ 这一段唯一的纪律：痛点显式留着，不许修

M1a 写完时，代码里必须还躺着下面这五处别扭。它们**就是 M1b 的全部原料** —— 提前修掉任何一处，M1b 就变成了"我告诉你该怎么写"，而不是"你自己发现问题"。

| # | 痛点 | 长什么样 |
|:--:|:---|:---|
| 1 | 窗口对象无处安放 | 一堆全局变量，或者把 `this` 硬塞进 `GWLP_USERDATA` |
| 2 | 加控件要改三处 | 一个控件 = 一个 ID 宏 + 一次 `CreateWindowExW` + `switch` 里一个 `case` |
| 3 | 消息处理散在 `WndProc` | 一个越长越长的 `switch`，业务逻辑和窗口管理混在一起 |
| 4 | 缩放后位置不会自己变 | `WM_SIZE` 里手算三个控件的坐标 |
| 5 | 状态改了界面不动 | 忘了 `InvalidateRect` 就花屏，忘了 `SetWindowText` 就不更新 |

---

## 二、M1b · 抽象：五个痛点，五层封装

顺序不能换：1 必须最先（后面所有东西都得有个对象来承载），2 依赖 1，3 / 4 / 5 依赖 2。

每一小节的写法固定四拍：**前身 → 痛点 → 抽象 → 落回具象**。

### 2.1 `Window` 对象 ← 痛点 1

- **前身**：全局变量，或 `GWLP_USERDATA` 里塞 `this`
- **痛点**：一个窗口还能凑合，两个就串味；`this` 拿不回来，全靠约定
- **抽象**：`Window` 类持有 HWND；HWND ↔ 对象建立唯一映射；`WM_NCCREATE` 时把 `this` 装进去（**必须在创建返回之前**，见穿插清单 B 组）
- **素材先例**：旧教程里 `08_global_state` → `09_instance_data` → `10_oop_wrapper` 三步走的就是这条路（原 `src/tutorial/native_win32/`，现已归档到私有的 `intern/legacy/src/native_win32/`）
- **落回具象**：`WndProc` 缩成一句 `Window::fromHwnd(hwnd)->dispatch(msg, wParam, lParam)`

### 2.2 `Widget` 树 ← 痛点 2

- **前身**：满屏控件 ID 宏 + `CreateWindowExW` + `switch case`
- **痛点**：加一个控件要改三处；父子关系只存在于代码的先后顺序里；销毁要手写
- **抽象**：`Widget` 基类 + 父子树（`std::unique_ptr` 持有子）+ ID 自动分配 + ID → 对象查表
- **落回具象**：树里从此出现两种节点 —— `NativeWidget`（背后是真子 HWND，像素由系统画）和自绘节点（像素由 `Painter` 画），详见第四节
- **注意**：这一步只解决"控件怎么进树"，不解决"事件怎么出来" —— 那是 2.3

### 2.3 事件与信号 ← 痛点 3

- **前身**：`WM_COMMAND` 里那个越来越长的 `switch`
- **痛点**：一个事件要通知多处时只能往 `case` 里继续堆
- **抽象**：`WM_COMMAND` / `WM_NOTIFY` → 类型化事件 → `Signal<T>` 派发给订阅者；`connect` 返回 `Connection`，对象析构时统一断开
- **落回具象**：`switch` 没了，变成 `button.onClick.connect([&]{ ... })`

### 2.4 布局 ← 痛点 4

- **前身**：`WM_SIZE` 里手算三个控件的坐标
- **痛点**：手算量随控件数量线性增长；改一个间距要全部重算
- **抽象**：`Layout` 接口 + Measure / Arrange 两遍；M1 只做竖排 `StackLayout`
- **落回具象**：`WM_SIZE` 里只剩一句 `layout_->arrange(root_, clientRect)`

### 2.5 属性与重画 ← 痛点 5

- **前身**：手工 `SetWindowText` + 手工 `InvalidateRect`
- **痛点**：状态散在各处，忘一次就花屏；"什么变了"没人知道
- **抽象**：`Property<T>` + `changed` 通知；属性变化 → 自动失效 → 自动重画
- **落回具象**：`label.text = ...` 之后再不用写任何 `InvalidateRect`

---

## 三、M1b 的终点：目标形态

> 这是**长出来的结果**，不是起点。读者走到这里，才第一次看见全景。

### 三层

| 层 | 角色 | 从哪一层长出来 |
|:---|:---|:---|
| `platform/` | 窗口、消息、输入、原生控件宿主 | 2.1 |
| `framework/` | 对象模型、控件树、布局、事件、状态 | 2.2 – 2.5 |
| `render/` | 绘制、文本、脏区 | 2.2 里的自绘节点 |

三个来源（Win32 / 造 GUI / 渲染）不是三本书，是同一根管子上的三个角色。**值钱的不是这三块，是块与块之间的四条契约** —— 框架藏起来的假设全在契约上。

### 四条契约

| 契约 | 一句话 | 谁提供 / 谁消费 |
|:---|:---|:---|
| **失效** | 控件说"我脏了"，但不知道要重画多大 | framework 上报，render 消费 |
| **坐标** | 布局用逻辑单位，只有 Window 知道 DPI | platform 提供 scale |
| **绘制** | 控件只发绘制指令，不碰设备 | render 隔离后端 |
| **生命周期** | 树是 `unique_ptr`，事件连接随对象销毁自动断 | framework 内部 |

**契约一 · 失效**

```cpp
// framework/widget.h
void invalidate(const Rect& r);   // 局部脏
void invalidateAll();             // 整体脏
```

- 控件**只上报"我脏了"**：不决定重画矩形、不碰像素、不认识后端
- 脏区沿父链冒泡到 `Window`，由 `Window` 合并裁剪后交给 Renderer
- **渲染层不认识控件**，只认识矩形

> M1 简化：`invalidateAll()` 直接整窗重画，但 `DamageList` 的结构先留着 —— M2 换真脏区时上层不改一行。

**契约二 · 坐标与 DPI**

- 布局、尺寸、间距一律用**逻辑单位（DIP）**
- `Window` 持有 `scale_`（per-monitor DPI v2 的当前缩放）
- 只有 Renderer 把 DIP 乘成物理像素；命中测试在 DIP 空间做
- 规则：**除了 Window，没人知道 DPI**

**契约三 · 绘制**

```cpp
// render/painter.h
class Painter {
public:
    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void clip(const Rect& r) = 0;
    virtual void fillRect(const Rect& r, Color c) = 0;
    virtual void drawText(const Rect& r, std::wstring_view s, const Font& f, Color c) = 0;
    virtual Size measureText(std::wstring_view s, const Font& f) = 0;
};

// framework/widget.h
virtual void onPaint(Painter& p, const Rect& damage) {}
```

- 控件只发**绘制指令**：不持 HDC、不持设备资源、**不 `#include <windows.h>`**
- `Painter` 是状态机（save / restore），不是一个 DC
- 后端可换：M1 是 `GdiPainter`，M2 是 `D2DPainter`，控件代码一行不动

**契约四 · 生命周期**

- 子控件：`std::unique_ptr`，父持有子，树形所有权
- 反向引用：M1 靠"销毁时自动断开的连接"（`Signal::connect` 返回 `Connection`，析构时统一断）；M2 升级成 `WeakPtrFactory`
- 规则：**任何回调都必须能在对象已死时安全返回**

---

## 四、混合树：原生控件在树里是什么节点

M1 用原生控件（`BUTTON` / `EDIT` / `LISTVIEW`），图的是快，以及白得输入法和无障碍。于是 M1 的控件树是**混合树**：

```
RootWindow            (HWND，自绘：背景 + 布局)
├─ Panel              (自绘：只做布局)
│   ├─ Label   → NativeWidget(STATIC 子 HWND)   ← 像素由系统画
│   └─ Editor  → NativeWidget(EDIT   子 HWND)   ← 像素由系统画
└─ CustomBanner       (自绘)                     ← 像素由我们的 Painter 画
```

`NativeWidget` 的职责：

- 持有一个子 HWND
- `onPaint` 为空 —— 系统窗口类自己画
- 参与布局：`arrange()` 里调 `SetWindowPos`
- 事件：把子 HWND 的 `WM_COMMAND` / `WM_NOTIFY` 翻译成框架事件

**这个混合状态本身就是一篇内容**：同一棵树里两种像素来源并存 —— 原生节点的像素是 `comctl32` 画的（系统消息 → 视觉样式 → 重定向表面），自绘节点的像素是我们画的。"控件如何上屏"在这里直接落成代码。

而它是接管轴的**代码落点**：M2 第 1 档把 `NativeWidget` 换成 `CustomWidget`（自绘），**树结构、布局、事件一行都不改** —— 这就是"把一根手指从 OS 手里掰开"的具体样子。

---

## 五、目录与文件树（M1b 终点 / M1c 用的形态）

```
gui/
├─ CMakeLists.txt
├─ app/
│   ├─ m1a_naked.cpp               M1a：裸 Win32 版（单文件，~250 行）
│   └─ demo.cpp                    M1c：框架版（同一个 demo）
├─ platform/
│   ├─ application.h/.cpp          消息循环 + 分发 + 消息 → 框架事件翻译
│   ├─ window.h/.cpp               HWND 生命周期、DPI、失效合并
│   └─ native_widget_host.h/.cpp   子 HWND 的创建 / 摆放 / 消息桥接
├─ framework/
│   ├─ geometry.h                  Point / Size / Rect（DIP）
│   ├─ signal.h                    Signal<T> + Connection
│   ├─ property.h                  Property<T> + changed（M1 极简，M2 升级为依赖感知）
│   ├─ widget.h/.cpp               树、父指针、invalidate、事件、onPaint
│   ├─ native_widget.h/.cpp        NativeWidget（M2 被 CustomWidget 替换）
│   └─ layout.h/.cpp               Layout 接口 + StackLayout
└─ render/
    ├─ painter.h                   Painter 接口 + Color / Font
    ├─ gdi_painter.h/.cpp          M1 上屏后端
    └─ damage.h                    DamageList（M1 整窗，形状先留）
```

**代码量上限**：M1 全部 ≤ 1500 行（不含注释与示例）。超过这个量就没法在一篇里讲完，架构就失去教学性。

---

## 六、M1c · 落回具象

用框架把同一个 demo 重写一遍，逐条对照：

| # | M1a 的做法 | M1c 的做法 |
|:--:|:---|:---|
| 1 | 全局变量 + `GWLP_USERDATA` 塞 `this` | `Application` + `Window` |
| 2 | ID 宏 + `CreateWindowExW` + `switch case` | `addChild<Button>(...)` |
| 3 | 长 `switch` 分发 | `button.onClick.connect(...)` |
| 4 | `WM_SIZE` 里手算坐标 | `StackLayout` |
| 5 | 手写 `SetWindowText` + `InvalidateRect` | `label.text = ...` |

**验收**：

1. 行为与 M1a 逐条一致（点击、缩放、DPI、中文、退出）
2. 代码行数显著更短
3. **加第四个控件，只改一行**

第 3 条是点睛：M1a 里加一个控件要改三处，M1c 里只改一处 —— **"为什么需要抽象"这个问题，读者自己就答出来了。**

---

## 七、M2 · 接管轴（M1 之后的课程目录）

| 档 | 接管什么 | 代码改哪 | 内容产出 |
|:--:|:---|:---|:---|
| 0 | 原生控件（M1 的状态） | — | 控件如何上屏（原生版） |
| 1 | 一个按钮改自绘 | `NativeWidget` → `CustomWidget` | 半自绘 → 全自绘；命中测试回归 |
| 2 | 上屏改自管 | `GdiPainter` → `D2DPainter` + DComp | 重定向表面消失之后 |
| 3 | 文本自己排版 | `measureText` 换 DWrite | 字体、行盒、CJK |
| 4 | 输入路由自己算 | 撤掉子 HWND，自己命中测试 | 事件路由、捕获、hover |
| 5 | 无障碍自己暴露 | 新增 UIA provider | 白得的东西要自己还 |
| 6 | 状态驱动重绘 | `Property` → 依赖感知内核 | 毛刺、拓扑序、只重画脏块 |

每一档都是"同一具树，换掉一个器官的实现"。**这张表就是 M1 之后的课程目录。**

> 接管的行为学解释（一个抽象控件到底要回答哪些问题、哪些从系统手里搬过来）见 [widget-behavior.md](widget-behavior.md)（控件行为清单）。

交付机制：**一份持续演进的代码库**，每个里程碑打一个 tag，读者 `git checkout m1` 就拿到那个时刻能跑的完整版本。

---

## 八、M1 明确不做（以及为什么）

| 不做 | 为什么 |
|:---|:---|
| 滚动的容器 | 要先把裁剪与命中测试讲透；那是 M2 第 4 档 |
| 主题 / 深色模式 | 依赖样式器官，M1 只有硬编码颜色 |
| 多窗口 | 会把 Application 变成窗口管理器，跑偏 |
| 动画 / 帧调度 | 需要帧循环概念，M1 只重画、不按帧 |
| 协程 | 需要消息循环先稳定；那是 M2 之后的事 |
| 依赖感知响应式 | M1 只要"改属性 → 重画"，毛刺是下一篇的题目 |
| 自绘控件 | 命中测试与 IME 会卡住进度；第一版故意不自绘 |
| IME 定制 | 原生控件白得，自绘才要自己还 —— 那是第 3、5 档 |
| Direct2D | M1 用 GDI 后端先跑起来；换后端是第 2 档的内容 |
| 无障碍 | 原生控件白得；自绘之后要自己暴露，是第 5 档 |

---

## 九、待拍板

1. **命名空间与项目名** —— `mw::` 已被 ModernWindows 占用，MiniUI 是 XCB 那条线，需要第三个名字或统一命名
2. **M1 上屏后端** —— 建议 GDI（换 D2D 是 M2 第 2 档的事，正好当内容）
3. **`Layout` 第一版只做竖排 `StackLayout`** —— 够不够撑起 M1a/M1c 的 demo
4. **M1a 的 demo 是否就用"输入框 + 按钮 + 文本"这一组** —— 它必须能撞上全部五条痛点
5. **ModernWindows 的最终位置** —— 理论篇与它现在是站点上仅存的内容；它在"器官主线"里是并列系列还是平台路线的收束，还没定
