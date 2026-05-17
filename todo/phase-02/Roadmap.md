# Phase 2 — 从零手搓一个 GUI 框架（MiniUI）

> **定位**：在 Linux（WSL2 + XLaunch/WSLg）上，用 XCB + Cairo 从零构建一个教学级 GUI 框架，通过亲手实现来理解 GUI 核心概念。
>
> **技术栈**：C++20 / XCB / Cairo / CMake / WSL2
>
> **替代**：原 Phase 2（08_phase2_gui_core.md）的多框架对比方案，改为实践驱动。

---

## 前置依赖

- Phase 1（Win32 原生编程，章节 0-61）已完成，提供 Win32 对比参照
- WSL2 环境，安装 XCB / Cairo 开发库
- XLaunch 或 WSLg 作为 X11 显示服务

---

## 总体设计

我们构建一个名为 **MiniUI** 的最小 GUI 框架。它不是生产级框架——它的唯一目的是让你理解每一个 GUI 框架在底层都做了什么。

```
读者的应用代码
    ↓
MiniUI 框架（我们手搓的部分）
  ├── Application    事件循环 + XCB 连接管理
  ├── Widget         控件树基类（父子关系、命中测试）
  ├── Layout         布局引擎（Measure → Arrange → Render）
  ├── Signal         信号/槽（控件间解耦通信）
  ├── Painter        Cairo RAII 封装（2D 绘制）
  └── DoubleBuffer   双缓冲 + 脏区管理
    ↓
XCB + Cairo（底层图形与窗口）
    ↓
X Server（XLaunch / WSLg）
```

---

## 章节（8 章，编号 62-69）

---

### Ch62：环境搭建与第一个 XCB 窗口

**文件**：`tutorial/gui_core/62_ProgrammingGUI_XcbWindow.md`
**代码**：`src/tutorial/gui_core/examples/01_xcb_window/`

#### 概念目标

- X11 客户端-服务器架构：X Client（我们的程序）、X Server（XLaunch/WSLg）、X Protocol
- 与 Win32 的横向对比（交叉引用 Ch0 的 GUI 四要素、Ch1 的 WinMain 骨架）
- XCB vs Xlib：为什么选 XCB（更底层、更薄的抽象）

#### WSL2 环境搭建

```bash
# 安装开发依赖
sudo apt update
sudo apt install -y libxcb1-dev libxcb-util-dev libcairo2-dev libcairo2 xcb pkg-config cmake g++

# 配置 DISPLAY（XLaunch 用户）
export DISPLAY=$(ip route list default | awk '{print $3}'):0.0
# WSLg 用户
export DISPLAY=:0

# 验证 X11 连通性
sudo apt install -y x11-apps
xeyes
```

#### 核心代码：最简 XCB 窗口（~100 行）

```cpp
#include <xcb/xcb.h>
#include <cstdio>
#include <cstdlib>

int main() {
    // 1. 连接到 X Server
    int screenNum;
    xcb_connection_t* conn = xcb_connect(nullptr, &screenNum);

    // 2. 获取屏幕信息
    const xcb_setup_t* setup = xcb_get_setup(conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screenNum; ++i)
        xcb_screen_next(&iter);
    xcb_screen_t* screen = iter.data;

    // 3. 创建窗口
    xcb_window_t win = xcb_generate_id(conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen->white_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
    };
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
                       0, 0, 800, 600, 0,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                       screen->root_visual, mask, values);

    xcb_map_window(conn, win);
    xcb_flush(conn);

    // 4. 事件循环
    xcb_generic_event_t* event;
    while ((event = xcb_wait_for_event(conn))) {
        switch (event->response_type & ~0x80) {
            case XCB_EXPOSE:
                printf("窗口需要重绘\n");
                break;
            case XCB_KEY_PRESS:
                printf("按键按下，退出\n");
                free(event);
                goto done;
        }
        free(event);
    }

done:
    xcb_disconnect(conn);
    return 0;
}
```

#### 并排对比：Win32 vs XCB 打开窗口

| 步骤 | Win32 | XCB |
|------|-------|-----|
| 初始化 | `GetModuleHandle(nullptr)` | `xcb_connect(nullptr, &screen)` |
| 注册类 | `RegisterClassEx(&wc)` | （不需要，X11 无窗口类概念） |
| 创建窗口 | `CreateWindowEx(...)` | `xcb_create_window(...)` |
| 显示 | `ShowWindow(hwnd, SW_SHOW)` | `xcb_map_window(conn, win)` |
| 事件循环 | `GetMessage(&msg, ...)` | `xcb_wait_for_event(conn)` |
| 分发 | `DispatchMessage(&msg)` | `switch (event->response_type)` |
| 清理 | `DestroyWindow(hwnd)` | `xcb_disconnect(conn)` |

#### 练习

- ⭐ 修改窗口标题（提示：`xcb_change_property` + `WM_NAME`）
- ⭐⭐ 添加 `XCB_KEY_PRESS` 处理：按 Escape 退出
- ⭐⭐⭐ 将 XCB 样板代码封装为一个 RAII 类 `XcbConnection`

---

### Ch63：事件循环——GUI 的心跳

**文件**：`tutorial/gui_core/63_ProgrammingGUI_EventLoop.md`
**代码**：`src/tutorial/gui_core/examples/02_event_loop/`
**框架新增**：`miniui/Application.hpp`（~150 行）

#### 概念目标

- 事件驱动编程的通用模型：`while(running) { event = getEvent(); dispatch(event); }`
- 事件 vs 消息 vs 信号：同一概念在不同框架中的命名
- XCB 事件类型与 Win32 消息的对应关系

#### 事件类型映射

| XCB 事件 | Win32 消息 | 含义 |
|----------|-----------|------|
| `XCB_EXPOSE` | `WM_PAINT` | 窗口需要重绘 |
| `XCB_BUTTON_PRESS` | `WM_LBUTTONDOWN` | 鼠标按下 |
| `XCB_BUTTON_RELEASE` | `WM_LBUTTONUP` | 鼠标释放 |
| `XCB_MOTION_NOTIFY` | `WM_MOUSEMOVE` | 鼠标移动 |
| `XCB_KEY_PRESS` | `WM_KEYDOWN` | 键盘按下 |
| `XCB_KEY_RELEASE` | `WM_KEYUP` | 键盘释放 |
| `XCB_CLIENT_MESSAGE` | 自定义 `WM_USER+N` | 自定义消息 |
| `XCB_CONFIGURE_NOTIFY` | `WM_SIZE` | 窗口大小变化 |

#### 框架设计：Application 类

```cpp
// miniui/Application.hpp
namespace miniui {

class Application {
public:
    Application();
    ~Application();  // xcb_disconnect

    int run();  // 主事件循环：xcb_wait_for_event → dispatch

    // 事件回调注册
    using EventCallback = std::function<void(xcb_generic_event_t*)>;
    void onEvent(uint8_t eventType, EventCallback cb);

    xcb_connection_t* connection() const;
    xcb_screen_t* screen() const;

    void quit();  // 设置 running_ = false

private:
    xcb_connection_t* conn_;
    xcb_screen_t* screen_;
    bool running_ = true;
    std::unordered_map<uint8_t, std::vector<EventCallback>> handlers_;
};

} // namespace miniui
```

**设计要点**：
- RAII 管理 `xcb_connection_t*`——构造时连接，析构时断开
- `run()` 封装事件循环，用户通过回调注册处理器
- 对标 Win32 的 `GetMessage` + `TranslateMessage` + `DispatchMessage` 三件套

#### 三框架事件循环伪代码对比

```
// Win32
while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

// XCB（我们的 Application::run）
while (running_) {
    event = xcb_wait_for_event(conn);
    dispatch(event);  // 根据 response_type 分发
}

// GTK
gtk_main();  // 内部也是 while 循环 + 事件分发

// 共同骨架
while (running) { event = wait(); dispatch(event); }
```

#### 练习

- ⭐ 在终端打印鼠标坐标和按键码
- ⭐⭐⭐ 用 `xcb_poll_for_event` 代替 `xcb_wait_for_event`，实现帧驱动循环（固定 60 FPS）
- ⭐⭐⭐⭐ 实现 `Timer` 类：利用 `xcb_conn_setup` 的 `xcb_gettimeofday` 在事件循环中周期性触发回调

---

### Ch64：绘制原语与 Cairo 集成

**文件**：`tutorial/gui_core/64_ProgrammingGUI_CairoDrawing.md`
**代码**：`src/tutorial/gui_core/examples/03_cairo_drawing/`
**框架新增**：`miniui/Painter.hpp`（~100 行）、`miniui/common.hpp`

#### 概念目标

- 渲染上下文：Cairo 的 `cairo_t` 对标 Win32 的 `HDC`（交叉引用 Ch18 GDI 基础）
- XCB 原生绘图的局限性 → 为什么需要更高级的绘图库
- 立即模式（Immediate Mode）vs 保留模式（Retained Mode）

#### XCB 原生绘图 vs Cairo

**XCB 能做的**：矩形、线条、多边形（纯色，无抗锯齿）
**XCB 做不到的**：文字渲染、抗锯齿、Alpha 混合、渐变、曲线

**Cairo** 是 GTK 的底层绘图库，提供：
- 矢量图形（路径、贝塞尔曲线）
- 文字渲染（FreeType 后端）
- Alpha 混合和渐变
- 多后端（XCB、PNG、PDF、SVG）

#### 框架设计：Painter 类

```cpp
// miniui/Painter.hpp
namespace miniui {

class Painter {
public:
    explicit Painter(cairo_t* cr);  // 不拥有 cairo_t
    ~Painter() = default;

    void setSourceColor(Color c);
    void setLineWidth(float w);

    void drawRect(Rect r);
    void fillRect(Rect r);
    void drawCircle(Point center, float radius);
    void fillCircle(Point center, float radius);
    void drawLine(Point a, Point b);
    void drawText(Point pos, const std::string& text,
                  const std::string& fontFamily = "sans", float fontSize = 14.0f);

    void clip(Rect r);
    void resetClip();
    void translate(float dx, float dy);
    void save();     // cairo_save
    void restore();  // cairo_restore

    cairo_t* nativeHandle() const;

private:
    cairo_t* cr_;
};

} // namespace miniui
```

**设计要点**：
- 轻量封装，不拥有 `cairo_t`（生命周期由 DoubleBuffer 或 RootWindow 管理）
- `save()`/`restore()` 对标 Cairo 的状态栈
- 类比 Win32 的 `ScopedSelectObject` 模式（交叉引用 Ch19）

#### 公共类型

```cpp
// miniui/common.hpp
namespace miniui {

struct Size  { int width = 0, height = 0; };
struct Point { int x = 0, y = 0; };
struct Rect  { int x = 0, y = 0, width = 0, height = 0; };
struct Color { float r = 0, g = 0, b = 0, a = 1.0f; };

constexpr Color White     {1.0f, 1.0f, 1.0f, 1.0f};
constexpr Color Black     {0.0f, 0.0f, 0.0f, 1.0f};
constexpr Color Transparent{0.0f, 0.0f, 0.0f, 0.0f};

} // namespace miniui
```

#### 练习

- ⭐ 用 Cairo 绘制一个彩色棋盘格
- ⭐⭐ 处理 `XCB_EXPOSE` 事件，窗口调整大小时重绘
- ⭐⭐⭐⭐ 用 Cairo 绘制一个带渐变背景和圆角矩形的"按钮"形状

---

### Ch65：控件树——Widget 抽象

**文件**：`tutorial/gui_core/65_ProgrammingGUI_WidgetTree.md`
**代码**：`src/tutorial/gui_core/examples/04_widget_tree/`
**框架新增**：`miniui/Widget.hpp`（~200 行）、`miniui/RootWindow.hpp`（~150 行）、`miniui/Label.hpp`（~50 行）

#### 概念目标

- 控件树的普遍性：Win32 HWND 树 / Qt QObject 树 / DOM 树 / 我们的 Widget 树——它们都是同一棵树
- 关键区别：Win32 每个 HWND 都是系统窗口；我们的方案是**单个 XCB 窗口 + 自绘控件树**
- 坐标变换、命中测试、事件传播

#### 四框架控件树对比

| 特性 | Win32 (HWND) | Qt (QObject) | Web (DOM) | MiniUI (Widget) |
|------|-------------|-------------|-----------|-----------------|
| 父子关系 | `SetParent` | `parent()` | DOM 嵌套 | `parent_` 指针 |
| 生命周期 | `DestroyWindow` | `delete` (所有权) | GC | `unique_ptr` 自动 |
| 坐标 | 屏幕坐标/客户区坐标 | `mapToGlobal` | `getBoundingClientRect` | 本地坐标 + 父偏移 |
| 事件路由 | `WM_PARENTNOTIFY` | 事件过滤器 | 冒泡/捕获 | 冒泡（子→父） |
| 自绘 | Owner-Draw | `paintEvent` | CSS | `paint(Painter&)` |

#### 框架设计：Widget 基类

```cpp
// miniui/Widget.hpp
namespace miniui {

class Widget {
public:
    Widget() = default;
    virtual ~Widget() = default;

    // === 树结构 ===
    Widget* parent() const;
    void addChild(std::unique_ptr<Widget> child);
    const std::vector<std::unique_ptr<Widget>>& children() const;

    // === 几何 ===
    Rect bounds() const;
    void setBounds(Rect r);
    Size size() const;
    Point position() const;

    // === 布局协议（Ch66 扩展） ===
    virtual Size measure(Size available);
    virtual void arrange(Rect allocated);

    // === 渲染 ===
    virtual void paint(Painter& painter) = 0;
    void repaint();  // 标记为脏

    // === 输入事件 ===
    virtual void onMousePress(Point pos, int button);
    virtual void onMouseRelease(Point pos, int button);
    virtual void onMouseMove(Point pos);
    virtual void onKeyPress(xcb_keycode_t keycode);

    // === 命中测试 ===
    virtual Widget* hitTest(Point pos);

    // === 可见性 ===
    void setVisible(bool v);
    bool visible() const;

protected:
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
    Rect bounds_;
    bool visible_ = true;
    bool dirty_ = true;
};

} // namespace miniui
```

**设计要点**：
- `unique_ptr<Widget>` 管理子控件生命周期，类比 Qt 的对象树所有权
- 命中测试从根节点向下遍历，找到最深层匹配的叶子控件
- 单继承，不使用 HWND——所有控件在同一个 XCB 窗口内自绘
- 对标 Win32 的 Owner-Draw 和完全自绘控件模式（交叉引用 Ch48-50）

#### RootWindow：XCB 窗口 + 控件树宿主

```cpp
// miniui/RootWindow.hpp
namespace miniui {

class RootWindow {
public:
    RootWindow(Application& app, int width = 800, int height = 600);
    ~RootWindow();

    void setRootWidget(std::unique_ptr<Widget> root);
    void show();
    void setTitle(const std::string& title);

    // 处理 XCB 原始事件，分派到控件树
    void handleXcbEvent(xcb_generic_event_t* event);

    // 全量重绘（后续 Ch68 优化为脏区合成）
    void repaintAll();

    xcb_window_t xcbWindow() const;

private:
    Application& app_;
    xcb_window_t window_;
    std::unique_ptr<Widget> rootWidget_;
    Size windowSize_;
};

} // namespace miniui
```

**设计要点**：
- 一个 `RootWindow` = 一个 `xcb_window_t` = 一个控件树根
- `handleXcbEvent` 将 XCB 事件翻译为 Widget 事件（坐标转换、命中测试）
- 对标 Win32 的 `HWND` + `WndProc`，但我们把所有控件逻辑集中在一个窗口内

#### Label 控件

```cpp
// miniui/Label.hpp
namespace miniui {

class Label : public Widget {
public:
    void setText(const std::string& text);
    void setColor(Color color);
    void setFontSize(float size);

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    std::string text_;
    Color color_ = Black;
    float fontSize_ = 14.0f;
};

} // namespace miniui
```

#### 练习

- ⭐ 创建三个不同颜色的 `Label`，手动设置 `setBounds` 排列它们
- ⭐⭐ 实现命中测试：点击某个 Label 时在终端打印 "clicked: [text]"
- ⭐⭐⭐⭐ 实现一个 `ColorBox` 控件：矩形，点击后随机变色（参考 Ch49 完全自绘 ButtonState 模式）

---

### Ch66：布局引擎——测量与排列

**文件**：`tutorial/gui_core/66_ProgrammingGUI_LayoutEngine.md`
**代码**：`src/tutorial/gui_core/examples/05_layout/`
**框架新增**：`miniui/Layout.hpp`、`miniui/HBoxLayout.hpp`（~80 行）、`miniui/VBoxLayout.hpp`（~80 行）、`miniui/Container.hpp`（~60 行）

#### 概念目标

- **为什么要布局引擎**：Win32 手动 `MoveWindow`（交叉引用 Ch4）→ 脆弱、不响应、无法组合
- **Measure → Arrange → Render** 三遍过程：这是所有 GUI 框架的通用管线
- 三种布局模型对比：绝对定位（Win32）、盒模型（CSS/GTK）、约束布局（Qt/SwiftUI）

#### 布局管线

```
                    可用空间 (800×600)
                          │
                    ┌─────▼─────┐
                    │  Measure  │ ← 每个 Widget 回报自己需要多大
                    └─────┬─────┘
                          │
                    ┌─────▼─────┐
                    │  Arrange  │ ← 根据策略分配实际位置和大小
                    └─────┬─────┘
                          │
                    ┌─────▼─────┐
                    │  Render   │ ← 在分配到的区域内绘制
                    └───────────┘
```

这与浏览器的工作方式完全相同：
1. **Measure**：CSS `width: auto` → 计算内容尺寸
2. **Arrange**：确定每个盒子的最终 x, y, width, height
3. **Render**：像素绘制

#### 框架设计

```cpp
// miniui/Layout.hpp — 布局策略接口
namespace miniui {

class Layout {
public:
    virtual ~Layout() = default;
    virtual Size measure(const std::vector<Widget*>& children, Size available) = 0;
    virtual void arrange(const std::vector<Widget*>& children, Rect allocated) = 0;
};

// miniui/Container.hpp — 拥有布局策略的 Widget
class Container : public Widget {
public:
    void setLayout(std::unique_ptr<Layout> layout);
    Size measure(Size available) override;
    void arrange(Rect allocated) override;
    void paint(Painter& painter) override;  // 递归绘制子控件

private:
    std::unique_ptr<Layout> layout_;
};

} // namespace miniui
```

#### HBoxLayout 实现（核心逻辑）

```cpp
// miniui/HBoxLayout.hpp
namespace miniui {

class HBoxLayout : public Layout {
public:
    Size measure(const std::vector<Widget*>& children, Size available) override {
        int totalWidth = 0;
        int maxHeight = 0;
        for (auto* child : children) {
            Size childSize = child->measure(available);
            totalWidth += childSize.width;
            maxHeight = std::max(maxHeight, childSize.height);
        }
        return {totalWidth, maxHeight};
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) override {
        int x = allocated.x;
        for (auto* child : children) {
            Size childPref = child->measure({allocated.width, allocated.height});
            child->arrange({x, allocated.y, childPref.width, allocated.height});
            x += childPref.width;
        }
    }
};

} // namespace miniui
```

#### 示例：响应式登录表单

```
┌─────────────────────────────────┐
│  VBoxLayout                     │
│  ┌─────────────────────────────┐│
│  │ HBoxLayout: [用户名:] [____]││
│  ├─────────────────────────────┤│
│  │ HBoxLayout: [密  码:] [____]││
│  ├─────────────────────────────┤│
│  │ HBoxLayout: [登录]  [取消]  ││
│  └─────────────────────────────┘│
└─────────────────────────────────┘
窗口缩放时，所有控件自动重新排列
```

#### 练习

- ⭐ 为 HBoxLayout 添加 `spacing`（控件间距）属性
- ⭐⭐ 实现 `Stretch` 因子：类似 CSS `flex-grow`，让某些控件占满剩余空间
- ⭐⭐⭐⭐ 实现嵌套布局：`VBoxLayout` 包含 `HBoxLayout` 包含控件——验证布局引擎的可组合性

---

### Ch67：信号系统与 MVx 模式

**文件**：`tutorial/gui_core/67_ProgrammingGUI_Signals.md`
**代码**：`src/tutorial/gui_core/examples/06_signals/`
**框架新增**：`miniui/Signal.hpp`（~70 行）、`miniui/Button.hpp`（~60 行）、`miniui/TextBox.hpp`（~50 行）

#### 概念目标

- 控件间如何解耦通信：Win32 的 `WM_COMMAND`（交叉引用 Ch5）vs Qt 的 Signal/Slot
- 观察者模式在 GUI 中的经典应用
- MVC / MVP / MVVM：三种架构模式的本质区别

#### Signal<Args...> 实现

```cpp
// miniui/Signal.hpp
namespace miniui {

class Connection {
public:
    void disconnect();
    int id() const;
private:
    int id_;
    // ... 内部引用 Signal 的 slot 列表
};

template <typename... Args>
class Signal {
public:
    Connection connect(std::function<void(Args...)> callback) {
        int id = nextId_++;
        slots_.push_back({id, std::move(callback)});
        return Connection{id};
    }

    void emit(Args... args) {
        for (auto& slot : slots_) {
            slot.callback(args...);
        }
    }

    void disconnect(int id) {
        std::erase_if(slots_, [id](const Slot& s) { return s.id == id; });
    }

private:
    struct Slot {
        int id;
        std::function<void(Args...)> callback;
    };
    std::vector<Slot> slots_;
    int nextId_ = 0;
};

} // namespace miniui
```

**设计要点**：
- `emit()` 是信号端，`connect()` 是槽端——发布-订阅
- `Connection` 允许断开连接（防止悬空回调）
- 不如 Qt 的 moc 高效，但教学环境足够清晰
- 对比 Win32：`WM_COMMAND` + 控件 ID 是"穷人版"的信号槽

#### Button 控件

```cpp
// miniui/Button.hpp
namespace miniui {

class Button : public Widget {
public:
    Signal<>& clicked() { return clicked_; }

    void setText(const std::string& text);
    Size measure(Size available) override;
    void paint(Painter& painter) override;
    void onMousePress(Point pos, int button) override;
    void onMouseRelease(Point pos, int button) override;

private:
    std::string text_;
    bool pressed_ = false;
    bool hovered_ = false;
    Signal<> clicked_;
};

} // namespace miniui
```

#### 迷你计算器（MVP 初步）

```
┌─────────────────────┐
│         42          │  ← Label (显示)
├─────────────────────┤
│ [7] [8] [9] [+]    │
│ [4] [5] [6] [-]    │  ← Button 网格
│ [1] [2] [3] [*]    │
│ [C] [0] [=] [/]    │
└─────────────────────┘

// 信号连接
button7->clicked().connect([&]() { display->setText(display->text() + "7"); });
```

#### MVx 模式对比

| 模式 | View 的角色 | 数据流 | 典型框架 |
|------|-----------|--------|---------|
| MVC | 被动显示 | Controller → Model → View | Web (Rails, Django) |
| MVP | 接口化 | Presenter ↔ View, Presenter ↔ Model | Win32/GTK 手动实现 |
| MVVM | 数据绑定 | ViewModel ←binding→ View | WinUI 3, Qt Quick |

**我们的框架适合 MVP**：因为 Signal 系统天然支持 Presenter ↔ View 的解耦。

#### 练习

- ⭐ 为 Button 添加 `hovered()` 和 `pressed()` 信号
- ⭐⭐⭐ 重构计算器为 MVP：`CalculatorModel`（纯数据）+ `CalculatorPresenter`（逻辑）+ `CalculatorView`（接口）
- ⭐⭐⭐⭐⭐ 实现信号断连的安全机制：Widget 销毁时自动 disconnect 所有关联的 Connection

---

### Ch68：渲染管线与脏区管理

**文件**：`tutorial/gui_core/68_ProgrammingGUI_RenderPipeline.md`
**代码**：`src/tutorial/gui_core/examples/07_render_pipeline/`
**框架新增**：`miniui/DoubleBuffer.hpp`（~60 行），更新 `Widget.hpp`（脏标记）

#### 概念目标

- 为什么"全窗口重绘"是浪费的
- 脏区（Dirty Region）机制：Win32 的 `InvalidateRect` + `WM_PAINT` 原理（交叉引用 Ch18, Ch23）
- 双缓冲消除闪烁

#### 脏区系统

```
                    ┌───────────────────┐
                    │  全窗口 (800×600) │
                    │                   │
                    │   ┌─────┐         │
                    │   │脏区1│  ┌───┐  │
                    │   └─────┘  │脏2│  │
                    │            └───┘  │
                    │                   │
                    └───────────────────┘

只重绘与脏区相交的控件，其余跳过
```

**Win32 对应**：
- `InvalidateRect(hwnd, &rect, FALSE)` → 标记脏区
- `BeginPaint` → 只允许在脏区绘制
- `ValidateRect` → 清除脏区
- `GetUpdateRect` → 查询脏区

#### 框架设计

```cpp
// miniui/DoubleBuffer.hpp
namespace miniui {

class DoubleBuffer {
public:
    DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                 uint16_t width, uint16_t height);
    ~DoubleBuffer();

    // 开始绘制：返回离屏 cairo_t，已裁剪到脏区
    Painter beginPaint(const std::vector<Rect>& dirtyRegions);

    // 结束绘制：将离屏 surface 拷贝到窗口
    void endPaint();

    void resize(uint16_t width, uint16_t height);

private:
    xcb_connection_t* conn_;
    xcb_window_t win_;
    cairo_surface_t* offscreen_;
    cairo_t* cr_;
    Size size_;
};

} // namespace miniui
```

#### Widget 脏标记更新

```cpp
// Widget.hpp 新增方法
void Widget::repaint() {
    dirty_ = true;
    // 通知 RootWindow 该区域需要重绘
    if (parent_) parent_->childDirty(this);
}

void Widget::update() {
    dirty_ = true;
    // 不立即重绘，标记为需要下次合成时处理
}
```

#### 示例：粒子系统性能对比

200 个弹跳粒子，每帧只重绘移动粒子的包围盒：
- 全窗口重绘：~30 FPS（800×600 全部重画）
- 脏区优化：~60 FPS（只重绘变化区域）

#### 练习

- ⭐ 为 `Label::setText` 添加正确的脏标记：新旧 bounds 都标记为脏
- ⭐⭐⭐ 优化粒子示例：将多个小脏矩形合并为一个大包围矩形（减少重绘次数）
- ⭐⭐⭐⭐⭐ 实现裁剪链：父控件 `cairo_clip` 到自身 bounds，子控件在其中绘制，确保子控件不会越界

---

### Ch69：收官——迷你文本编辑器

**文件**：`tutorial/gui_core/69_ProgrammingGUI_MiniEditor.md`
**代码**：`src/tutorial/gui_core/examples/08_mini_editor/`（~450 行）
**框架新增**：无（纯应用代码）

#### 概念目标

- MVP 架构的完整实践
- 回顾 MiniUI 框架全貌
- 与 Qt/GTK 的差距分析：我们的教学框架缺什么

#### 架构

```
┌─────────────────────────────────────────────┐
│                  EditorPresenter             │
│  ┌──────────┐                    ┌────────┐ │
│  │   Model  │◄───信号/调用──────►│  View   │ │
│  │          │                    │        │ │
│  │TextBuffer│                    │Editor  │ │
│  │ lines[]  │                    │ View   │ │
│  │ undoStack│                    │        │ │
│  │ redoStack│                    │MenuBar │ │
│  │          │                    │TextCvs │ │
│  └──────────┘                    │StatBar │ │
│                                  └────────┘ │
└─────────────────────────────────────────────┘
```

#### Model：TextBuffer

```cpp
// TextBuffer.hpp — 纯数据，无 GUI 代码
class TextBuffer {
public:
    void insertChar(char c);
    void deleteChar();
    void newLine();
    void undo();
    void redo();
    bool loadFile(const std::string& path);
    bool saveFile(const std::string& path);

    const std::vector<std::string>& lines() const;
    size_t cursorLine() const;
    size_t cursorCol() const;
    bool isModified() const;

    Signal<>& contentChanged() { return changed_; }

private:
    std::vector<std::string> lines_{""};
    size_t cursorLine_ = 0, cursorCol_ = 0;
    std::vector<std::string> undoStack_, redoStack_;
    bool modified_ = false;
    Signal<> changed_;
};
```

#### View：EditorView

组合控件：
- **MenuBar**：`HBoxLayout`，包含 Button（新建/打开/保存/退出）
- **TextCanvas**：自绘 Widget，使用 Cairo 绘制文字行 + 光标 + 选区
- **StatusBar**：`Label`，显示行号、列号、字符数

#### Presenter：EditorPresenter

```cpp
// EditorPresenter.hpp
class EditorPresenter {
public:
    EditorPresenter(TextBuffer& model, EditorView& view);

    // 连接信号
    void setup();

private:
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onCharInput(xcb_keycode_t key);
    void onContentChanged();

    TextBuffer& model_;
    EditorView& view_;
};
```

#### 一次按键的完整链路

```
1. XCB_KEY_PRESS 事件
2. Application::run() 从事件循环取出
3. RootWindow::handleXcbEvent(event)
4. 命中测试 → 找到 TextCanvas
5. TextCanvas::onKeyPress(keycode)
6. 发射 Signal<char> charTyped
7. EditorPresenter::onCharInput(char)
8. TextBuffer::insertChar(char)
9. TextBuffer 发射 Signal<> contentChanged
10. EditorPresenter::onContentChanged()
11. TextCanvas::repaint() → 标记脏区
12. RootWindow::compose() → DoubleBuffer::beginPaint
13. TextCanvas::paint(Painter&) → Cairo 绘制文字
14. DoubleBuffer::endPaint() → 拷贝到窗口
```

#### 我们的框架 vs Qt/GTK

| 特性 | MiniUI | Qt | GTK |
|------|--------|----|-----|
| 窗口系统 | XCB 单窗口 | 多平台抽象 | Gdk/X11/Wayland |
| 渲染 | Cairo CPU | QPainter / RHI | Cairo / Vulkan |
| 布局 | BoxLayout | QLayout 系列 | GtkBox/Grid/Grid |
| 信号系统 | `Signal<>` | moc Signal/Slot | GObject Signals |
| 主题/皮肤 | 无 | QSS / QPalette | CSS |
| DPI 缩放 | 无 | 自动 | 自动 |
| 无障碍 | 无 | QAccessible | ATK |
| 多线程 | 不安全 | 信号跨线程 | `g_idle_add` |
| GPU 加速 | 无 | RHI/Vulkan | Vulkan |

**我们缺的不是技术能力，而是工程化**——这些差距将在后续篇章（Phase 3-7）中逐步补齐。

#### 练习

- ⭐ 添加行号显示和字数统计到状态栏
- ⭐⭐⭐ 实现 C++ 关键字语法高亮（`int`, `class`, `return`... 用不同颜色）
- ⭐⭐⭐⭐⭐ 实现查找/替换功能：弹出搜索框，高亮匹配结果

---

## 代码目录结构

```
tutorial/gui_core/                              # 教程 Markdown
  index.md
  62_ProgrammingGUI_XcbWindow.md
  63_ProgrammingGUI_EventLoop.md
  64_ProgrammingGUI_CairoDrawing.md
  65_ProgrammingGUI_WidgetTree.md
  66_ProgrammingGUI_LayoutEngine.md
  67_ProgrammingGUI_Signals.md
  68_ProgrammingGUI_RenderPipeline.md
  69_ProgrammingGUI_MiniEditor.md

src/tutorial/gui_core/                          # 代码示例
  CMakeLists.txt                                # 顶层：find_package(XCB, Cairo)
  miniui/                                       # 框架头文件
    common.hpp                                  # Ch64: Size, Rect, Point, Color
    Application.hpp                             # Ch63: 事件循环 + XCB 连接
    Painter.hpp                                 # Ch64: Cairo RAII 封装
    Widget.hpp                                  # Ch65: 控件基类 (Ch68 增加脏标记)
    RootWindow.hpp                              # Ch65: XCB 窗口 + 控件树宿主
    Label.hpp                                   # Ch65: 文本标签控件
    Layout.hpp                                  # Ch66: 布局策略接口
    HBoxLayout.hpp                              # Ch66: 水平盒布局
    VBoxLayout.hpp                              # Ch66: 垂直盒布局
    Container.hpp                               # Ch66: 容器控件
    Signal.hpp                                  # Ch67: 信号/槽
    Button.hpp                                  # Ch67: 按钮控件
    TextBox.hpp                                 # Ch67: 文本输入控件
    DoubleBuffer.hpp                            # Ch68: 双缓冲 + 脏区合成
  examples/
    01_xcb_window/        CMakeLists.txt + main.cpp
    02_event_loop/         CMakeLists.txt + main.cpp
    03_cairo_drawing/      CMakeLists.txt + main.cpp
    04_widget_tree/        CMakeLists.txt + main.cpp
    05_layout/             CMakeLists.txt + main.cpp
    06_signals/            CMakeLists.txt + main.cpp
    07_render_pipeline/    CMakeLists.txt + main.cpp
    08_mini_editor/        CMakeLists.txt + main.cpp + TextBuffer.hpp + EditorView.hpp + EditorPresenter.hpp + TextCanvas.hpp
```

---

## 框架演进时间线

| 章节 | miniui/ 新增内容 | 示例演示 |
|------|-----------------|---------|
| 62 | （无框架，纯 XCB） | 空白窗口 |
| 63 | `Application.hpp` | 事件打印到终端 |
| 64 | `common.hpp`, `Painter.hpp` | Cairo 形状 + 渐变 + 文字 |
| 65 | `Widget.hpp`, `RootWindow.hpp`, `Label.hpp` | 三个 Label + 命中测试 |
| 66 | `Layout.hpp`, `HBoxLayout.hpp`, `VBoxLayout.hpp`, `Container.hpp` | 响应式登录表单 |
| 67 | `Signal.hpp`, `Button.hpp`, `TextBox.hpp` | 迷你计算器 (MVP) |
| 68 | `DoubleBuffer.hpp`, 更新 `Widget.hpp` | 粒子系统 (性能对比) |
| 69 | （无新框架文件） | 迷你文本编辑器 |

---

## CMake 结构

### 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MiniUITutorials VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 通过 pkg-config 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb xcb-util)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xcb)

# 框架头文件目录
set(MINIUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/miniui)

add_compile_options(-Wall -Wextra -Wpedantic)

# 按章节添加示例
add_subdirectory(examples/01_xcb_window)
add_subdirectory(examples/02_event_loop)
add_subdirectory(examples/03_cairo_drawing)
add_subdirectory(examples/04_widget_tree)
add_subdirectory(examples/05_layout)
add_subdirectory(examples/06_signals)
add_subdirectory(examples/07_render_pipeline)
add_subdirectory(examples/08_mini_editor)
```

### 单个示例 CMakeLists.txt 模板

```cmake
project(01_xcb_window)

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${MINIUI_DIR} ${XCB_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${XCB_LIBRARIES} ${CAIRO_LIBRARIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${XCB_CFLAGS} ${CAIRO_CFLAGS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

---

## VitePress 集成（后续步骤）

以下文件需要修改以支持 `gui_core` 目录，**不在本 Roadmap 的执行范围内**，列为后续 TODO：

### sidebar.ts 需新增

```typescript
const GUI_CORE_GROUPS: ChapterGroup[] = [
  { title: '环境与基础', prefix: '62' },
  { title: '框架核心', prefix: '65' },
  { title: '综合实战', prefix: '69' },
]
```

在 `buildSidebar()` 中添加对 `tutorial/gui_core/` 的扫描，复用现有分组逻辑。

### nav.ts 需新增

```typescript
{ text: 'GUI 核心概念', link: '/gui_core/' }
```

### tutorial/index.md 需新增

第二篇 feature 卡片：
```yaml
- title: GUI 核心概念
  details: 8 章 · 从零手搓 MiniUI 框架 · XCB + Cairo · Linux 实战
  link: /gui_core/62_ProgrammingGUI_XcbWindow
```

---

## 验证方案

### 每章完成后

```bash
# 1. 编译验证
cd src/tutorial/gui_core
cmake -B build && cmake --build build

# 2. 运行示例
./build/bin/XX_example_name

# 3. 视觉确认（手动）
# - 窗口是否正确显示
# - 事件是否正确响应
# - 布局是否正确排列
```

### 全部完成后

```bash
# 1. 所有 8 个示例都能编译运行
cmake --build build
for exe in build/bin/*; do echo "Testing $exe"; timeout 3 $exe || true; done

# 2. VitePress 构建无错误
cd /path/to/project/root
pnpm run build

# 3. 增量完整性：miniui/ 所有头文件无 breaking change
# 后一章的示例能正常包含前面所有头文件
```

### CI 集成（后续）

在 GitHub Actions 中添加 Linux 构建 job：
- 安装 `libxcb1-dev`, `libcairo2-dev`
- CMake 构建 `src/tutorial/gui_core/`
- 注意：CI 无法验证 GUI 显示，只验证编译通过
