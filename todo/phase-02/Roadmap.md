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

## 章节（11 章，编号 62-72）

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

### Ch65：控件树 + RAII 资源管理

**文件**：`tutorial/hands_on_ur_own_gui/handbook/65_widget_tree.md`（待写）
**代码**：`src/tutorial/anatomy_gui_for_tutorials/stage2/example/04_widget_tree/`
**框架新增**：`miniui/Widget.hpp`、`miniui/RootWindow.hpp`、`miniui/Label.hpp`、`base/raii.hpp`、`platform/xcb_raii.hpp`、`platform/cairo_raii.hpp`

#### 概念目标

- 控件树的普遍性：Win32 HWND 树 / Qt QObject 树 / DOM 树 / 我们的 Widget 树——它们都是同一棵树
- 关键区别：Win32 每个 HWND 都是系统窗口；我们的方案是**单个 XCB 窗口 + 自绘控件树**
- 坐标变换、命中测试、事件传播

#### 现代 C++ 融入：RAII

**引出问题**：Ch64 里手动 `cairo_create/cairo_destroy`，控件树要管理几十个资源，异常路径必漏——需要一个通用的资源管理方案。

- `unique_ptr` + 自定义 deleter 封装 `xcb_connection_t*`、`cairo_surface_t*`、`cairo_t*`
- Move-only 资源类型：为什么 copy 被禁止，move 合理
- `std::scope_exit`：事件处理中的临时资源清理
- Rule of Zero / Rule of Five
- Win32 横向对比：`HWND`/`HDC`/`HBRUSH` 同样是手动管理句柄，`ScopedSelectObject` 是 RAII 前身

#### 框架设计：RAII 包装（新增）

```cpp
// base/raii.hpp — 通用 RAII 工具
struct XcbConnectionDeleter {
    void operator()(xcb_connection_t* conn) const {
        if (conn) xcb_disconnect(conn);
    }
};
using XcbConnectionPtr = std::unique_ptr<xcb_connection_t, XcbConnectionDeleter>;

// platform/xcb_raii.hpp
class XcbConnection {
    XcbConnectionPtr conn_;
public:
    XcbConnection() : conn_(xcb_connect(nullptr, nullptr)) {}
    ~XcbConnection() = default;  // 自动 disconnect
    XcbConnection(const XcbConnection&) = delete;
    XcbConnection& operator=(const XcbConnection&) = delete;
    XcbConnection(XcbConnection&&) = default;
    // ...
};
```

**从 Ch65 起，miniui/ 不再出现裸的 `*_destroy` 调用。**

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

### Ch66：布局引擎 + Concepts——测量与排列

**文件**：`tutorial/gui_core/66_ProgrammingGUI_LayoutEngine.md`
**代码**：`src/tutorial/gui_core/examples/05_layout/`
**框架新增**：`miniui/Layout.hpp`（concept）、`miniui/HBoxLayout.hpp`（~80 行）、`miniui/VBoxLayout.hpp`（~80 行）、`miniui/Container.hpp`（template，~60 行）

#### 概念目标

- **为什么要布局引擎**：Win32 手动 `MoveWindow`（交叉引用 Ch4）→ 脆弱、不响应、无法组合
- **Measure → Arrange → Render** 三遍过程：这是所有 GUI 框架的通用管线
- 三种布局模型对比：绝对定位（Win32）、盒模型（CSS/GTK）、约束布局（Qt/SwiftUI）

#### 现代 C++ 融入：Concepts

**引出问题**：Ch65 里 `Container` 持有 `unique_ptr<Layout>` 基类指针，每次 `measure`/`arrange` 都是虚函数调用——但布局策略在编译期就确定了（一个 `HBoxLayout` 永远是水平排列），虚函数的分发开销和不必要的类型擦除是多余的。我们能不能让编译器在编译期就选择正确的布局策略？

**C++20 Concepts** 正好解决这个问题：

- `LayoutStrategy` concept：约束模板参数必须提供 `measure(...)` 和 `arrange(...)` 方法
- `Container<LayoutPolicy>` 类模板：布局策略作为模板参数，编译期静态分发
- 编译期接口检查：如果传入的类型不满足 concept，编译器给出清晰的错误信息
- 与旧方案的对比：`virtual` 动态分发 vs concept 模板静态分发——零开销抽象

```cpp
// miniui/Layout.hpp — 布局策略 concept（替代虚基类）
namespace miniui {

// Concept：约束"可布局"类型
template <typename T>
concept LayoutStrategy = requires(T t, const std::vector<Widget*>& children,
                                   Size available, Rect allocated) {
    { t.measure(children, available) } -> std::same_as<Size>;
    { t.arrange(children, allocated) } -> std::same_as<void>;
};

// Container<LayoutPolicy> — 布局策略作为模板参数，静态分发
template <LayoutStrategy LayoutPolicy>
class Container : public Widget {
public:
    explicit Container(LayoutPolicy policy = {})
        : layout_(std::move(policy)) {}

    Size measure(Size available) override {
        return layout_.measure(rawChildren(), available);
    }

    void arrange(Rect allocated) override {
        layout_.arrange(rawChildren(), allocated);
        // 递归 arrange 子控件
        for (auto& child : children_) {
            child->arrange(child->bounds());
        }
    }

    void paint(Painter& painter) override {
        for (auto& child : children_) {
            if (child->visible()) {
                painter.save();
                painter.clip(child->bounds());
                child->paint(painter);
                painter.restore();
            }
        }
    }

private:
    LayoutPolicy layout_;   // 不是指针——直接嵌入，零堆分配

    std::vector<Widget*> rawChildren() const {
        std::vector<Widget*> result;
        for (auto& c : children_) result.push_back(c.get());
        return result;
    }
};

} // namespace miniui
```

**框架代码变更**：
- `Layout.hpp`：删除 `class Layout` 虚基类，改为 `LayoutStrategy` concept
- `Container.hpp`：从 `Container` 虚基类改为 `Container<LayoutPolicy>` 模板
- `HBoxLayout.hpp` / `VBoxLayout.hpp`：删除 `: public Layout`，改为普通类（concept 自动检查）

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

#### HBoxLayout 实现（满足 LayoutStrategy concept）

```cpp
// miniui/HBoxLayout.hpp
namespace miniui {

class HBoxLayout {
public:
    explicit HBoxLayout(int spacing = 0) : spacing_(spacing) {}

    Size measure(const std::vector<Widget*>& children, Size available) {
        int totalWidth = 0;
        int maxHeight = 0;
        for (auto* child : children) {
            Size childSize = child->measure(available);
            totalWidth += childSize.width;
            maxHeight = std::max(maxHeight, childSize.height);
        }
        totalWidth += spacing_ * std::max(0, (int)children.size() - 1);
        return {totalWidth, maxHeight};
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) {
        int x = allocated.x;
        for (auto* child : children) {
            Size childPref = child->measure({allocated.width, allocated.height});
            child->arrange({x, allocated.y, childPref.width, allocated.height});
            x += childPref.width + spacing_;
        }
    }

private:
    int spacing_ = 0;
};

// 编译期验证：HBoxLayout 满足 LayoutStrategy concept
static_assert(LayoutStrategy<HBoxLayout>);

} // namespace miniui
```

#### 示例：响应式登录表单

```cpp
// 使用 Container<HBoxLayout> 和 Container<VBoxLayout> 组合
auto form = std::make_unique<Container<VBoxLayout>>(VBoxLayout{8});

auto row1 = std::make_unique<Container<HBoxLayout>>(HBoxLayout{4});
row1->addChild(std::make_unique<Label>("用户名:"));
row1->addChild(std::make_unique<TextBox>());

auto row2 = std::make_unique<Container<HBoxLayout>>(HBoxLayout{4});
row2->addChild(std::make_unique<Label>("密  码:"));
row2->addChild(std::make_unique<TextBox>());

auto buttons = std::make_unique<Container<HBoxLayout>>(HBoxLayout{8});
buttons->addChild(std::make_unique<Button>("登录"));
buttons->addChild(std::make_unique<Button>("取消"));

form->addChild(std::move(row1));
form->addChild(std::move(row2));
form->addChild(std::move(buttons));
```

```
┌─────────────────────────────────┐
│  VBoxLayout (spacing=8)         │
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

- ⭐ 为 `HBoxLayout` 添加 `stretch` 因子：类似 CSS `flex-grow`，让某些控件占满剩余空间
- ⭐⭐⭐ 实现自定义布局策略：编写 `GridLayout` 类，验证它自动满足 `LayoutStrategy` concept
- ⭐⭐⭐⭐ 实现嵌套布局：`Container<VBoxLayout>` 包含 `Container<HBoxLayout>` 包含控件——验证布局引擎的可组合性，并对比旧虚函数方案的编译输出（`nm` 查看虚表）

---

### Ch67：信号系统 + 类型安全模板——MVx 模式

**文件**：`tutorial/gui_core/67_ProgrammingGUI_Signals.md`
**代码**：`src/tutorial/gui_core/examples/06_signals/`
**框架新增**：`miniui/Signal.hpp`（concept 版，~100 行）、`miniui/Property.hpp`（~50 行）、`miniui/Button.hpp`（~60 行）、`miniui/TextBox.hpp`（~50 行）

#### 概念目标

- 控件间如何解耦通信：Win32 的 `WM_COMMAND`（交叉引用 Ch5）vs Qt 的 Signal/Slot
- 观察者模式在 GUI 中的经典应用
- MVC / MVP / MVVM：三种架构模式的本质区别

#### 现代 C++ 融入：类型安全模板

**引出问题**：Ch66 的布局引擎用 `std::function` 回调没问题，但当信号系统也用 `std::function<void(Args...)>` 存储回调时，问题来了——`std::function` 是类型擦除的运行时多态，每次 `emit()` 都有一次虚调用开销。更严重的是：Ch65-66 里控件属性变化（`setText`、`setBounds`）都是手动调 `repaint()`，经常忘记——能不能让属性变化自动触发重绘？

**解决方案**：

- `std::invocable<Args...>` concept 约束回调类型，替代 `std::function` 的类型擦除
- `Property<T>` 模板类：封装"值 + 变化信号"，赋值即触发通知
- `Signal<Args...>` 用完美转发减少不必要的拷贝
- `connect()` 返回 RAII `ScopedConnection`，析构自动断连

```cpp
// miniui/Signal.hpp — concept 约束的类型安全信号系统
namespace miniui {

// Concept：约束可调用对象
template <typename F, typename... Args>
concept SlotCallable = std::invocable<F, Args...>;

// RAII 连接：析构自动断连
class ScopedConnection {
public:
    ScopedConnection() = default;
    ScopedConnection(std::function<void()> disconnector, int id)
        : disconnect_(std::move(disconnector)), id_(id) {}
    ~ScopedConnection() { if (disconnect_) disconnect_(); }
    ScopedConnection(ScopedConnection&& other) noexcept
        : disconnect_(std::move(other.disconnect_)), id_(other.id_) {
        other.disconnect_ = nullptr;
    }
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    void disconnect() { if (disconnect_) { disconnect_(); disconnect_ = nullptr; } }
private:
    std::function<void()> disconnect_;
    int id_ = -1;
};

template <typename... Args>
class Signal {
public:
    template <SlotCallable<Args...> F>
    ScopedConnection connect(F&& callback) {
        int id = nextId_++;
        slots_.push_back({id, std::forward<F>(callback)});
        auto* slots = &slots_;
        return ScopedConnection(
            [slots, id]() {
                std::erase_if(*slots, [id](const Slot& s) { return s.id == id; });
            }, id);
    }

    void emit(Args... args) {
        for (auto& slot : slots_) {
            slot.callback(args...);
        }
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

```cpp
// miniui/Property.hpp — 响应式属性：赋值即通知
namespace miniui {

template <typename T>
class Property {
public:
    Property() = default;
    Property(T initial) : value_(std::move(initial)) {}

    // 赋值时自动触发信号
    Property& operator=(const T& newValue) {
        if (value_ != newValue) {
            value_ = newValue;
            changed_.emit(value_);
        }
        return *this;
    }

    const T& get() const { return value_; }
    operator const T&() const { return value_; }

    // 监听变化
    Signal<const T&>& changed() { return changed_; }

private:
    T value_{};
    Signal<const T&> changed_;
};

} // namespace miniui
```

**框架代码变更**：
- `Signal.hpp`：用 `SlotCallable` concept 替代裸 `std::function`，添加 `ScopedConnection`
- `Property.hpp`（新增）：响应式值包装，属性变化自动通知
- `Button.hpp`：`text_` 改为 `Property<std::string>`，赋值自动触发重绘

#### Button 控件（使用 Property）

```cpp
// miniui/Button.hpp
namespace miniui {

class Button : public Widget {
public:
    Signal<>& clicked() { return clicked_; }

    // Property：赋值自动触发 changed 信号
    Property<std::string>& text() { return text_; }

    Size measure(Size available) override;
    void paint(Painter& painter) override;
    void onMousePress(Point pos, int button) override;
    void onMouseRelease(Point pos, int button) override;

    Button() {
        // 属性变化 → 自动标记脏区
        text_.changed().connect([this](const auto&) { repaint(); });
    }

private:
    Property<std::string> text_;
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

// 信号连接（使用 ScopedConnection 自动管理生命周期）
auto conn = button7->clicked().connect([&]() {
    display->text() = display->text().get() + "7";
});
```

#### MVx 模式对比

| 模式 | View 的角色 | 数据流 | 典型框架 |
|------|-----------|--------|---------|
| MVC | 被动显示 | Controller → Model → View | Web (Rails, Django) |
| MVP | 接口化 | Presenter ↔ View, Presenter ↔ Model | Win32/GTK 手动实现 |
| MVVM | 数据绑定 | ViewModel ←binding→ View | WinUI 3, Qt Quick |

**我们的框架适合 MVP**：因为 `Signal` + `Property<T>` 系统天然支持 Presenter ↔ View 的解耦，且 `Property` 已经为 MVVM 的数据绑定打下基础。

#### 练习

- ⭐ 为 `TextBox` 添加 `Property<std::string> content`，输入时自动通知
- ⭐⭐⭐ 重构计算器为 MVP：`CalculatorModel`（纯数据）+ `CalculatorPresenter`（逻辑）+ `CalculatorView`（接口），使用 `Property<T>` 传递显示值
- ⭐⭐⭐⭐ 实现信号断连的安全机制：利用 `ScopedConnection` 的 RAII 特性，Widget 析构时自动断开所有连接

---

### Ch68：渲染管线 + 异步 IO——双缓冲与脏区管理

**文件**：`tutorial/gui_core/68_ProgrammingGUI_RenderPipeline.md`
**代码**：`src/tutorial/gui_core/examples/07_render_pipeline/`
**框架新增**：`miniui/DoubleBuffer.hpp`（~60 行）、`miniui/AsyncFile.hpp`（~80 行），更新 `Application.hpp`（事件循环演进），更新 `Widget.hpp`（脏标记）

#### 概念目标

- 为什么"全窗口重绘"是浪费的
- 脏区（Dirty Region）机制：Win32 的 `InvalidateRect` + `WM_PAINT` 原理（交叉引用 Ch18, Ch23）
- 双缓冲消除闪烁

#### 现代 C++ 融入：Async IO

**引出问题**：Ch67 的迷你计算器一切正常——但如果我们想做一个"图片浏览器"，`loadFile()` 读取一个 10MB 的 PNG 会怎样？事件循环被阻塞，整个 GUI 冻结直到读取完成。XCB 事件队列里堆积了大量未处理的鼠标/键盘事件，用户看到窗口"卡死"。我们需要在事件循环中同时处理 IO 和 GUI 事件。

**解决方案**：

- `poll()` 多路复用：同时监听 XCB 文件描述符和自定义文件描述符
- `xcb_get_file_descriptor()` 获取 XCB 连接的 fd
- `Application::postTask()` 将 IO 完成回调投递到事件循环
- `AsyncFile` 封装非阻塞文件读取

```cpp
// miniui/AsyncFile.hpp — 非阻塞文件 IO
namespace miniui {

class AsyncFile {
public:
    using ReadCallback = std::function<void(std::vector<uint8_t>)>;

    // 异步读取整个文件，完成后回调被投递到 Application 事件循环
    static void readAll(Application& app, const std::string& path,
                        ReadCallback callback) {
        // 在后台线程读取
        std::thread([path, callback, &app]() {
            // ... 读取文件 ...
            std::vector<uint8_t> data = /* read file */;

            // 通过 postTask 回到事件循环线程
            app.postTask([data = std::move(data), cb = std::move(callback)]() {
                cb(std::move(data));
            });
        }).detach();
    }
};

} // namespace miniui
```

```cpp
// Application.hpp 演进：poll() 多路复用 + postTask
namespace miniui {

class Application {
public:
    // ... 原有接口 ...

    // 向事件循环投递任务（线程安全）
    void postTask(std::function<void()> task);

    // 演进后的事件循环
    int run() {
        int xcbFd = xcb_get_file_descriptor(conn_);
        while (running_) {
            // 1. 先处理所有待处理的任务
            while (!taskQueue_.empty()) {
                auto task = taskQueue_.pop_front();
                task();
            }

            // 2. poll 等待事件
            struct pollfd pfd = { xcbFd, POLLIN, 0 };
            poll(&pfd, 1, 16);  // ~60 FPS

            // 3. 处理 XCB 事件
            while (auto* event = xcb_poll_for_event(conn_)) {
                dispatch(event);
                free(event);
            }

            // 4. 合成脏区
            compose();
        }
        return 0;
    }

private:
    std::mutex queueMutex_;
    std::deque<std::function<void()>> taskQueue_;
};

} // namespace miniui
```

**Win32 横向对比**：
- `poll()` + `xcb_fd` ≈ `MsgWaitForMultipleObjectsEx()` 同时等待消息和 IO
- `Application::postTask()` ≈ `PostMessage(hwnd, WM_USER+N, ...)` 跨线程通知
- `AsyncFile` 的线程模型 ≈ Win32 的 `OVERLAPPED` + `ReadFileEx` + 完成端口

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

#### 框架设计：DoubleBuffer

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

#### 示例：粒子系统 + 异步图片加载

200 个弹跳粒子（脏区优化）+ 异步加载背景图片：
- 全窗口重绘：~30 FPS（800×600 全部重画）
- 脏区优化：~60 FPS（只重绘变化区域）
- 异步加载：图片在后台线程解码，完成后 `postTask` 回到事件循环渲染，GUI 不卡顿

#### 练习

- ⭐ 为 `Label::setText` 添加正确的脏标记：新旧 bounds 都标记为脏
- ⭐⭐⭐ 用 `AsyncFile::readAll()` 实现异步加载 PNG 并显示（提示：Cairo 可以从内存 buffer 创建 surface）
- ⭐⭐⭐⭐⭐ 实现裁剪链：父控件 `cairo_clip` 到自身 bounds，子控件在其中绘制，确保子控件不会越界

---

### Ch69：迷你文本编辑器 + 协程——MVP 架构完整实践

**文件**：`tutorial/gui_core/69_ProgrammingGUI_MiniEditor.md`
**代码**：`src/tutorial/gui_core/examples/08_mini_editor/`（~600 行）
**框架新增**：`miniui/Task.hpp`（~120 行），更新 `Application.hpp`（协程调度器）

#### 概念目标

- MVP 架构的完整实践
- 回顾 MiniUI 框架全貌
- 与 Qt/GTK 的差距分析：我们的教学框架缺什么

#### 现代 C++ 融入：协程（Coroutines）

**引出问题**：Ch68 的 `AsyncFile::readAll()` 解决了 IO 阻塞问题，但回调地狱（callback hell）出现了——打开文件 → 读取内容 → 解析语法 → 更新 UI，四层嵌套的 `postTask` 回调让代码变成了"箭头形"。我们需要一种方式，让异步代码看起来像同步代码一样线性。

**C++20 协程** 正好解决这个问题：

- `Task<T>` 协程返回类型：代表一个可能异步完成的计算
- `co_await async_load_file()`：看起来像同步调用，实际挂起协程、让出事件循环
- 协程调度器集成到 `Application::run()`：挂起的协程在 IO 完成后自动恢复
- 对比 Ch68 的回调方案：同样的功能，代码从嵌套 4 层变成线性 4 行

```cpp
// miniui/Task.hpp — 极简协程 Task
namespace miniui {

template <typename T = void>
class Task {
public:
    struct promise_type {
        T value_;
        std::coroutine_handle<> continuation_;
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        auto final_suspend() noexcept {
            struct Awaiter {
                std::coroutine_handle<> continuation;
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                    return continuation ? continuation : std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return Awaiter{continuation_};
        }
        void return_value(T value) { value_ = std::move(value); }
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    // ... operator co_await, get(), etc.
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> continuation);
    T await_resume();

private:
    std::coroutine_handle<promise_type> handle_;
};

} // namespace miniui
```

```cpp
// Application.hpp 演进：协程调度器
namespace miniui {

class Application {
public:
    // ... 原有接口 ...

    // 协程调度：IO 完成后恢复挂起的协程
    void scheduleResume(std::coroutine_handle<> handle) {
        postTask([handle]() { handle.resume(); });
    }

    int run() {
        // ... poll 事件循环 ...
        // 每轮循环末尾处理 postTask 队列中的协程恢复
    }
};

} // namespace miniui
```

**协程版的异步加载**：

```cpp
// 回调版（Ch68）——嵌套回调
AsyncFile::readAll(app, "big_file.txt", [&](auto data) {
    parseSyntax(data, [&](auto tokens) {
        app.postTask([&, tokens]() {
            textCanvas->setContent(tokens);
        });
    });
});

// 协程版（Ch69）——线性代码
Task<> EditorPresenter::openFile(const std::string& path) {
    auto data = co_await async_load_file(path);    // 挂起，让出事件循环
    auto tokens = parseSyntax(data);               // 恢复后继续
    view_->setContent(tokens);                      // 更新 UI
    view_->statusBar()->text() = "Loaded: " + path;
}
```

**框架代码变更**：
- `Task.hpp`（新增）：`Task<T>` 协程返回类型 + promise_type
- `Application.hpp`：添加 `scheduleResume()` 方法，postTask 队列同时处理普通回调和协程恢复

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
- **MenuBar**：`Container<HBoxLayout>`，包含 Button（新建/打开/保存/退出）
- **TextCanvas**：自绘 Widget，使用 Cairo 绘制文字行 + 光标 + 选区
- **StatusBar**：`Label`（`Property<std::string>` 自动更新），显示行号、列号、字符数

#### Presenter：EditorPresenter（使用协程）

```cpp
// EditorPresenter.hpp
class EditorPresenter {
public:
    EditorPresenter(TextBuffer& model, EditorView& view);

    // 连接信号
    void setup();

private:
    // 协程版的文件操作——线性代码，异步执行
    miniui::Task<> onOpenFile();
    miniui::Task<> onSaveFile();
    void onNewFile();
    void onCharInput(xcb_keycode_t key);
    void onContentChanged();

    TextBuffer& model_;
    EditorView& view_;
};
```

#### 一次按键的完整链路

```
1. XCB_KEY_PRESS 事件
2. Application::run() 从 poll 事件循环取出
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
| 布局 | Concept 模板 | QLayout 系列 | GtkBox/Grid/Grid |
| 信号系统 | `Signal<>` + concept | moc Signal/Slot | GObject Signals |
| 异步 IO | `poll()` + 协程 | QSocketNotifier | GMainContext |
| 主题/皮肤 | 无 | QSS / QPalette | CSS |
| DPI 缩放 | 无 | 自动 | 自动 |
| 无障碍 | 无 | QAccessible | ATK |
| 多线程 | postTask | 信号跨线程 | `g_idle_add` |
| GPU 加速 | 无 | RHI/Vulkan | Vulkan |

**我们缺的不是技术能力，而是工程化**——这些差距将在后续篇章（Phase 3-7）中逐步补齐。

#### 练习

- ⭐ 添加行号显示和字数统计到状态栏
- ⭐⭐⭐ 用 `co_await async_load_file()` 实现异步打开文件：大文件加载时 GUI 不卡顿
- ⭐⭐⭐⭐⭐ 实现 C++ 关键字语法高亮（`int`, `class`, `return`... 用不同颜色），语法解析放在协程中异步执行

---

### Ch70：响应式事件流 + Rx 模式——Observable

**文件**：`tutorial/gui_core/70_ProgrammingGUI_Observable.md`
**代码**：`src/tutorial/gui_core/examples/09_observable/`
**框架新增**：`miniui/Observable.hpp`（~150 行），更新 `Widget.hpp`（添加 Observable 事件源）

#### 概念目标

- 拖拽选择、多手势交互：多个事件流的组合（按下 + 移动 + 释放 = 拖拽）
- 为什么 Signal 不够用：Signal 是"发射一次处理一次"，但拖拽需要组合三个事件流
- Rx（Reactive Extensions）模式在 GUI 中的威力

#### 现代 C++ 融入：Rx 模式（Observable）

**引出问题**：Ch67 的 Signal 系统 + Ch69 的协程已经能处理大多数场景，但遇到"拖拽选择"就力不从心了——一个拖拽操作 = `onMousePress` 开始 + 连续的 `onMouseMove` 更新 + `onMouseRelease` 结束。用 Signal 写出来是三个回调加一堆状态变量，用协程也需要手动管理"取消"。我们需要一种组合事件流的方式。

**Rx（Reactive Extensions）** 模式：

- `Observable<T>`：事件流抽象——一个可能产生多个值的源
- `filter`：只保留满足条件的事件
- `map`：变换事件类型
- `take_until`：一个流在另一个流发射时结束
- `combine_latest`：合并多个流的最新值

```cpp
// miniui/Observable.hpp — Rx 风格的事件流
namespace miniui {

template <typename T>
class Observable {
public:
    using Observer = std::function<void(T)>;
    using Unsubscribe = std::function<void()>;

    Unsubscribe subscribe(Observer observer) {
        int id = nextId_++;
        observers_.push_back({id, std::move(observer)});
        return [this, id]() {
            std::erase_if(observers_, [id](const auto& o) { return o.id == id; });
        };
    }

    void next(T value) {
        for (auto& obs : observers_) {
            obs.callback(value);
        }
    }

    // === 操作符 ===

    // filter: 只保留满足谓词的值
    auto filter(std::function<bool(const T&)> predicate)
        -> Observable<T>;

    // map: 变换值类型
    template <typename U>
    auto map(std::function<U(const T&)> transform)
        -> Observable<U>;

    // take_until: 在 until 发射时停止
    auto take_until(Observable<any>& until)
        -> Observable<T>;

    // combine_latest: 合并两个流的最新值
    template <typename U>
    auto combine_latest(Observable<U>& other)
        -> Observable<std::pair<T, U>>;

private:
    struct Entry { int id; Observer callback; };
    std::vector<Entry> observers_;
    int nextId_ = 0;
};

} // namespace miniui
```

#### Widget 集成 Observable

```cpp
// Widget.hpp 演进：事件源变成 Observable
class Widget {
public:
    // 原有虚函数接口保留（向后兼容）
    // 新增：Observable 事件源
    Observable<Point>& mousePress()   { return mousePress_; }
    Observable<Point>& mouseMove()    { return mouseMove_; }
    Observable<Point>& mouseRelease() { return mouseRelease_; }

private:
    Observable<Point> mousePress_;
    Observable<Point> mouseMove_;
    Observable<Point> mouseRelease_;
};
```

#### 示例：多手势绘图画布

```cpp
// 拖拽绘图 = press → 连续 move → release
auto dragStream = canvas->mouseMove()
    .take_until(canvas->mouseRelease());

canvas->mousePress().subscribe([&, dragStream](Point start) mutable {
    auto path = std::make_shared<std::vector<Point>>();
    path->push_back(start);

    // 每次 move 都把点加入路径并重绘
    auto unsub = dragStream.subscribe([path](Point pos) {
        path->push_back(pos);
        canvas->addStroke(*path);
    });

    // release 时停止
    canvas->mouseRelease().subscribe([unsub](auto) {
        unsub();  // 取消 move 订阅
    });
});
```

#### 练习

- ⭐ 用 `filter` 实现：只在 Shift 按住时响应鼠标移动
- ⭐⭐⭐ 实现双击检测：两次 `mousePress` 在 300ms 内 = 双击事件
- ⭐⭐⭐⭐⭐ 实现完整的绘图工具：画笔、橡皮擦、颜色选择器——全部用 Observable 组合事件流

---

### Ch71：线程池与并发 GUI

**文件**：`tutorial/gui_core/71_ProgrammingGUI_ThreadPool.md`
**代码**：`src/tutorial/gui_core/examples/10_threadpool/`
**框架新增**：`miniui/ThreadPool.hpp`（~120 行），更新 `Application.hpp`（持有线程池）

#### 概念目标

- 重计算阻塞 UI：缩略图生成、文件搜索、图片滤镜——为什么不能在主线程做
- GUI 线程安全原则：只有事件循环线程能操作 Widget
- Actor 模型：通过消息传递而非共享状态实现并发

#### 现代 C++ 融入：线程池与并发

**引出问题**：Ch68 的 `AsyncFile` 用 `std::thread` + detach 处理简单 IO，但多线程裸写的问题暴露了——10 个缩略图同时生成 = 10 个线程，没有上限、没有取消、没有复用。Ch69 的协程解决了代码可读性，但协程本身不是线程——它还是单线程的。我们需要一个结构化的并发方案。

**C++20 线程库增强**：

- `std::jthread`：自动 join 的线程 + `std::stop_token` 协作取消
- 线程池：固定数量工作线程 + 任务队列
- `Application` 持有线程池，提供 `runAsync()` 接口
- `postTask` 作为"回到 GUI 线程"的唯一通道

```cpp
// miniui/ThreadPool.hpp — 固定大小线程池
namespace miniui {

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 提交任务，返回 future（但推荐用 postTask 回到 GUI 线程）
    template <typename F>
    auto submit(F&& task) -> std::future<std::invoke_result_t<F>> {
        using Return = std::invoke_result_t<F>;
        auto promise = std::make_shared<std::promise<Return>>();
        auto future = promise->get_future();

        {
            std::lock_guard lock(mutex_);
            tasks_.push([p = std::move(promise), t = std::forward<F>(task)]() mutable {
                try {
                    if constexpr (std::is_void_v<Return>) {
                        t();
                        p->set_value();
                    } else {
                        p->set_value(t());
                    }
                } catch (...) {
                    p->set_exception(std::current_exception());
                }
            });
        }
        cv_.notify_one();
        return future;
    }

    void shutdown();

private:
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

} // namespace miniui
```

```cpp
// Application.hpp 演进：持有线程池
class Application {
public:
    // 在线程池中执行任务，完成后 postTask 回到 GUI 线程
    template <typename F, typename Callback>
    void runAsync(F&& work, Callback&& onDone) {
        pool_.submit([this, work = std::forward<F>(work),
                      onDone = std::forward<Callback>(onDone)]() mutable {
            auto result = work();
            postTask([result = std::move(result), onDone = std::move(onDone)]() mutable {
                onDone(std::move(result));
            });
        });
    }

    ThreadPool& threadPool() { return pool_; }

private:
    ThreadPool pool_;
};
```

**Win32 横向对比**：
- `ThreadPool::submit()` ≈ `CreateThreadpoolWork()` + `SubmitThreadpoolWork()`
- `Application::postTask()` ≈ `PostMessage(hwnd, WM_USER+N, ...)` 回到 UI 线程
- `std::jthread` + `stop_token` ≈ `CreateThread()` + `TerminateThread()`（但协作式取消更安全）

#### 示例：文件浏览器 + 缩略图

```
┌──────────────────────────────────────────┐
│ Path: /home/user/pictures/    [Browse]   │
├──────────────────────────────────────────┤
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │
│ │ img1 │ │ img2 │ │ img3 │ │ img4 │    │
│ │ 🔄   │ │ 🔄   │ │ ✅   │ │ ✅   │    │
│ └──────┘ └──────┘ └──────┘ └──────┘    │
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │
│ │ img5 │ │ img6 │ │ img7 │ │ img8 │    │
│ │ ⏳   │ │ ⏳   │ │ ⏳   │ │ ⏳   │    │
│ └──────┘ └──────┘ └──────┘ └──────┘    │
├──────────────────────────────────────────┤
│ Status: Loading 2/8 thumbnails...        │
└──────────────────────────────────────────┘

缩略图在线程池中异步生成，完成后逐个回到 GUI 线程更新
```

#### 练习

- ⭐ 实现"取消加载"功能：用 `stop_token` 取消正在生成的缩略图
- ⭐⭐⭐ 实现带进度条的批量图片滤镜：线程池处理 + `postTask` 更新进度条
- ⭐⭐⭐⭐⭐ 实现 Actor 模型：`ThumbnailActor` 封装状态 + 消息队列，线程安全地与 GUI 交互

---

### Ch72：Win32 现代封装（Bonus·大门）

**文件**：`tutorial/gui_core/72_ProgrammingGUI_Win32Wrapping.md`
**代码**：`src/tutorial/gui_core/examples/11_win32_wrapping/`
**框架新增**：`modern_win32/` 命名空间（并行代码库，不修改 miniui）

#### 概念目标

- 回归 Win32：用现代 C++ 重新封装 Phase 1 学过的 Win32 API
- RAII 句柄、类型安全 WndProc、协程异步——这些模式不只适用于 XCB
- **大门**：从我们的手搓模式理解 WinUI 3 / COM 的设计哲学

#### 现代 C++ 融入：综合 Win32 封装

**引出问题**：Phase 1（Ch0-61）的 Win32 代码充满裸句柄、手动 `CloseHandle`、`WndProc` 的巨型 switch、`OVERLAPPED` 回调地狱——所有我们在 MiniUI 中用 RAII / concept / 协程解决的问题，Win32 代码里都存在。让我们用学到的方法来"治愈"Win32。

**五个维度的现代封装**：

1. **RAII 句柄**（Ch65 技能）：`UniqueHandle<HICON, DestroyIcon>` 替代裸 `HICON`
2. **类型安全 WndProc → Observable**（Ch67/70 技能）：`WM_LBUTTONDOWN` 变成 `mouseDown().subscribe(...)`
3. **协程异步**（Ch69 技能）：`co_await ReadFileAsync()` 替代 `OVERLAPPED` + 回调
4. **线程安全 UI**（Ch71 技能）：`DispatcherQueue::Post()` 回到 UI 线程
5. **完整示例**：Win32 文件浏览器——缩略图 + 异步加载 + Observable 事件

```cpp
// modern_win32/UniqueHandle.hpp — RAII 句柄封装
namespace modern_win32 {

template <typename Handle, auto Deleter>
class UniqueHandle {
public:
    explicit UniqueHandle(Handle h = nullptr) : handle_(h) {}
    ~UniqueHandle() { if (handle_) Deleter(handle_); }

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        reset(other.release());
        return *this;
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    Handle get() const { return handle_; }
    operator bool() const { return handle_ != nullptr; }
    Handle release() { auto h = handle_; handle_ = nullptr; return h; }
    void reset(Handle h = nullptr) { if (handle_) Deleter(handle_); handle_ = h; }

private:
    Handle handle_;
};

// 常用句柄类型别名
using UniqueWindow  = UniqueHandle<HWND, DestroyWindow>;
using UniqueIcon    = UniqueHandle<HICON, DestroyIcon>;
using UniqueBrush   = UniqueHandle<HBRUSH, DeleteObject>;
using UniquePen     = UniqueHandle<HPEN, DeleteObject>;
using UniqueBitmap  = UniqueHandle<HBITMAP, DeleteObject>;
using UniqueFile    = UniqueHandle<HANDLE, CloseHandle>;

} // namespace modern_win32
```

```cpp
// modern_win32/Window.hpp — Observable WndProc
namespace modern_win32 {

class Window {
public:
    auto& mouseDown()  { return mouseDown_; }
    auto& mouseUp()    { return mouseUp_; }
    auto& mouseMove()  { return mouseMove_; }
    auto& keyPress()   { return keyPress_; }
    auto& paint()      { return paint_; }
    auto& resize()     { return resize_; }

    void create(const std::wstring& title, int w, int h);
    void show();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    Observable<Point>    mouseDown_;
    Observable<Point>    mouseUp_;
    Observable<Point>    mouseMove_;
    Observable<int>      keyPress_;
    Observable<Size>     paint_;
    Observable<Size>     resize_;
};

} // namespace modern_win32
```

#### "大门"——从手搓模式到现代 GUI 框架

我们的 MiniUI 模式在 WinRT/WinUI 3 中都有工业级对应：

| MiniUI 概念 | WinRT / WinUI 3 对应 | 说明 |
|------------|---------------------|------|
| `UniqueHandle` | `winrt::com_ptr<T>` | COM 引用计数智能指针 |
| `Signal<>/Property<T>` | `winrt::event` / `DependencyProperty` | WinRT 内建的属性变更通知 |
| `Task<T>` 协程 | `IAsyncOperation<T>` | WinRT 原生协程支持 |
| `Application::postTask()` | `DispatcherQueue::TryEnqueue()` | 回到 UI 线程的标准方式 |
| `Observable<T>` | `IObservableVector<T>` / `IObservableMap<T>` | WinRT 集合变更通知 |
| `ThreadPool` | `Windows::System::Threading::ThreadPool` | WinRT 线程池 |
| `LayoutStrategy` concept | `Panel` + `MeasureOverride`/`ArrangeOverride` | XAML 布局系统 |
| `Widget` 树 | `DependencyObject` 树 | XAML 可视化树 |

**看到这些对应关系，你就理解了——WinUI 3 不是魔法，它就是我们手搓的这些模式的工业级实现。**

#### 练习

- ⭐ 用 `UniqueHandle` 重写 Phase 1 中任意一个示例，对比代码量变化
- ⭐⭐⭐ 实现 `Window` 类：用 Observable WndProc 替代 switch-case，写一个响应鼠标绘图的窗口
- ⭐⭐⭐⭐⭐ 用所有 modern_win32 组件实现 Win32 文件浏览器：异步缩略图 + Observable 事件 + 协程 IO

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
  70_ProgrammingGUI_Observable.md
  71_ProgrammingGUI_ThreadPool.md
  72_ProgrammingGUI_Win32Wrapping.md

src/tutorial/gui_core/                          # 代码示例
  CMakeLists.txt                                # 顶层：find_package(XCB, Cairo)
  miniui/                                       # 框架头文件
    common.hpp                                  # Ch64: Size, Rect, Point, Color
    Application.hpp                             # Ch63: 事件循环 + XCB 连接 (Ch68: poll + postTask, Ch69: 协程调度器, Ch71: 线程池)
    Painter.hpp                                 # Ch64: Cairo RAII 封装
    Widget.hpp                                  # Ch65: 控件基类 (Ch68: 脏标记, Ch70: Observable 事件源)
    RootWindow.hpp                              # Ch65: XCB 窗口 + 控件树宿主
    Label.hpp                                   # Ch65: 文本标签控件
    Layout.hpp                                  # Ch66: LayoutStrategy concept
    HBoxLayout.hpp                              # Ch66: 水平盒布局 (满足 LayoutStrategy)
    VBoxLayout.hpp                              # Ch66: 垂直盒布局 (满足 LayoutStrategy)
    Container.hpp                               # Ch66: Container<LayoutPolicy> 模板
    Signal.hpp                                  # Ch67: SlotCallable concept + ScopedConnection
    Property.hpp                                # Ch67: Property<T> 响应式属性
    Button.hpp                                  # Ch67: 按钮控件 (使用 Property)
    TextBox.hpp                                 # Ch67: 文本输入控件
    DoubleBuffer.hpp                            # Ch68: 双缓冲 + 脏区合成
    AsyncFile.hpp                               # Ch68: 非阻塞文件 IO
    Task.hpp                                    # Ch69: Task<T> 协程返回类型
    Observable.hpp                              # Ch70: Observable<T> + Rx 操作符
    ThreadPool.hpp                              # Ch71: 固定大小线程池 + jthread
  modern_win32/                                 # Ch72: Win32 现代封装 (并行代码库)
    UniqueHandle.hpp                            # Ch72: RAII 句柄封装
    Window.hpp                                  # Ch72: Observable WndProc
    AsyncIO.hpp                                 # Ch72: 协程异步 IO
    Dispatcher.hpp                              # Ch72: 线程安全 UI 调度
  examples/
    01_xcb_window/        CMakeLists.txt + main.cpp
    02_event_loop/         CMakeLists.txt + main.cpp
    03_cairo_drawing/      CMakeLists.txt + main.cpp
    04_widget_tree/        CMakeLists.txt + main.cpp
    05_layout/             CMakeLists.txt + main.cpp
    06_signals/            CMakeLists.txt + main.cpp
    07_render_pipeline/    CMakeLists.txt + main.cpp
    08_mini_editor/        CMakeLists.txt + main.cpp + TextBuffer.hpp + EditorView.hpp + EditorPresenter.hpp + TextCanvas.hpp
    09_observable/         CMakeLists.txt + main.cpp + DrawingCanvas.hpp
    10_threadpool/         CMakeLists.txt + main.cpp + FileBrowser.hpp + ThumbnailActor.hpp
    11_win32_wrapping/     CMakeLists.txt + main.cpp (Win32 示例，需 Windows 构建)
```

---

## 框架演进时间线

| 章节 | miniui/ 新增内容 | 融入的现代 C++ | 示例演示 |
|------|-----------------|----------------|---------|
| 62 | （无框架，纯 XCB） | 无 | 空白窗口 |
| 63 | `Application.hpp` | 无 | 事件打印到终端 |
| 64 | `common.hpp`, `Painter.hpp` | 无 | Cairo 形状 + 渐变 + 文字 |
| 65 | `Widget.hpp`, `RootWindow.hpp`, `Label.hpp`, `rai.hpp`, `xcb_raii.hpp`, `cairo_raii.hpp` | RAII | 三个 Label + 命中测试 |
| 66 | `Layout.hpp`(concept), `HBoxLayout.hpp`, `VBoxLayout.hpp`, `Container.hpp`(template) | Concepts | 响应式登录表单 |
| 67 | `Signal.hpp`(concept), `Property.hpp`(new), `Button.hpp`, `TextBox.hpp` | Type-safe templates | 迷你计算器 (MVP) |
| 68 | `DoubleBuffer.hpp`, `AsyncFile.hpp`, `Application` evolve | Async IO | 粒子系统 + 异步图片加载 |
| 69 | `Task.hpp`(new), `Application` coroutine scheduler | Coroutines | 迷你文本编辑器 |
| 70 | `Observable.hpp`(new), Widget add observables | Rx patterns | 多手势绘图画布 |
| 71 | `ThreadPool.hpp`(new), `Application` owns pool | Concurrency | 文件浏览器 + 缩略图 |
| 72 | `modern_win32/` namespace (parallel codebase) | Win32 wrapping (综合) | Win32 文件浏览器 |

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
add_subdirectory(examples/09_observable)
add_subdirectory(examples/10_threadpool)
# Ch72 Win32 示例仅 Windows 可构建
if(WIN32)
    add_subdirectory(examples/11_win32_wrapping)
endif()
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
  { title: '现代 C++ 模式', prefix: '70' },
  { title: 'Win32 封装', prefix: '72' },
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
  details: 11 章 · 从零手搓 MiniUI 框架 · XCB + Cairo + 现代 C++ · Linux 实战
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
# 1. 所有 11 个示例都能编译运行（Linux: 10 个, Win32: 1 个）
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
