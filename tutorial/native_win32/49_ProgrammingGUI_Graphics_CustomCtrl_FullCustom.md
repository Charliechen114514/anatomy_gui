# 通用GUI编程技术——图形渲染实战（四十九）——完全自绘控件架构：状态机与动画

> 上一篇文章我们讨论了 Owner-Draw 自绘技术——利用 `WM_DRAWITEM` 消息在系统控件的框架内自定义外观。但 Owner-Draw 终究是在别人家的地基上装修，控件的很多行为（焦点管理、键盘导航、无障碍接口）仍然由系统掌控。当我们需要一个彻底定制外观和行为的控件时——比如一个带平滑过渡动画的 Toggle 开关、一个环形进度条、或者一个完全自定义的滑块——我们就必须跳出 Owner-Draw 的框架，从零开始构建一个完全自绘控件。

完全自绘控件不是一个新概念，但它确实是 GUI 编程中一块硬骨头。它需要我们同时处理四个维度的问题：视觉状态的管理（状态机）、用户输入的响应（鼠标与键盘）、视觉反馈的呈现（绘制），以及辅助功能的支持（无障碍接口）。今天这篇文章我们聚焦在前三个维度上，通过一个完整的自绘 Button 控件框架来拆解这套架构。

## 环境说明

在开始之前，先说明一下我们的开发环境：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **图形API**: Direct2D (硬件加速 2D 渲染)
- **目标平台**: Win32 API 原生开发，不依赖 MFC 或其他框架

我们选择 Direct2D 而非 GDI 来做绘制，原因很简单——Direct2D 提供硬件加速、原生支持抗锯齿、Alpha 混合、以及与 DirectWrite 的高质量文字渲染配合。对于一个需要平滑动画的自绘控件来说，这些都是刚性需求。

## 自绘控件的四要素

在动手写代码之前，我们先来理清完全自绘控件需要处理的核心问题。所谓"四要素"并不是什么官方说法，而是我们在实践中总结出来的必要组成部分——它们分别是状态机、命中测试、键盘导航和无障碍接口。

状态机是自绘控件的灵魂。一个按钮控件至少有 Normal、Hover、Pressed、Disabled、Focused 五种状态，每种状态对应不同的视觉效果（颜色、边框、阴影等）。状态之间的切换可能由鼠标事件触发（Normal -> Hover -> Pressed），也可能由键盘事件触发（Focused + Space -> Pressed），甚至可能由程序逻辑触发（Enabled -> Disabled）。如果我们不把状态管理好，控件就会出现绘制与实际行为不一致的尴尬局面。

命中测试解决的是"鼠标点击的是不是我"这个问题。对于矩形控件来说这看起来简单，但对于非矩形控件（比如圆形按钮、环形进度条）或者重叠控件来说，精确的命中测试至关重要。命中测试的细节我们会在下一篇文章中深入讨论，这里先用简单的矩形判断。

键盘导航让控件可以在没有鼠标的情况下被操作。Tab 键切换焦点、Space 或 Enter 触发点击——这些是 Windows 无障碍体验的基本要求。如果我们做一个自绘控件却不处理键盘事件，那这个控件在实际产品中是不合格的。

无障碍接口（UI Automation）让屏幕阅读器等辅助工具能够理解控件的角色、状态和值。这一块比较复杂，我们今天不会深入实现，但会在代码框架中留出扩展点。

## 第一步——定义控件状态机

我们从状态机的定义开始。一个清晰的状态枚举是整个控件架构的基石。

```cpp
// CustomButton.h
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// 控件视觉状态
enum class ButtonState
{
    Normal,     // 默认状态，鼠标不在控件上
    Hover,      // 鼠标悬停在控件上
    Pressed,    // 鼠标按下（左键）
    Disabled,   // 控件被禁用
    Focused     // 控件拥有键盘焦点（但鼠标未悬停、未按下）
};
```

你可能注意到 `Focused` 状态和 `Hover`/`Pressed` 状态存在一定的互斥关系——确实如此。一个控件可以同时拥有焦点且被鼠标悬停，这时候应该展示什么状态？我们的策略是按照优先级排序：`Disabled` 最高（禁用时忽略一切其他状态），其次是 `Pressed`，再其次是 `Hover`，最后是 `Focused` 或 `Normal`。这个优先级逻辑在后面的状态计算函数中会体现出来。

接下来定义一个辅助函数来计算当前应该展示的状态。注意这里我没有简单地用一个成员变量来存储状态，而是根据多个布尔标志来"推导"当前状态——这样做的好处是状态之间不会出现不一致的情况：

```cpp
// 根据多个布尔标志推导当前视觉状态
ButtonState ComputeVisualState() const
{
    if (!m_enabled)
        return ButtonState::Disabled;

    if (m_pressed)
        return ButtonState::Pressed;

    if (m_hovering)
        return ButtonState::Hover;

    if (m_focused)
        return ButtonState::Focused;

    return ButtonState::Normal;
}
```

你会发现这种"推导式"的状态计算比"赋值式"更不容易出bug——因为我们不需要在每个事件处理函数中都小心翼翼地维护一个状态变量，只要确保各个布尔标志被正确设置就行了。每个事件处理函数只负责更新自己管辖的那一个标志，状态的计算交给统一的函数来完成。

## 第二步——自绘 Button 类框架

现在我们来构建自绘 Button 类的完整框架。这个类需要管理 Direct2D 的渲染资源、处理鼠标和键盘消息、维护状态机，并在 `WM_PAINT` 时完成绘制。

```cpp
// CustomButton.h（续）
class CustomButton
{
public:
    CustomButton();
    ~CustomButton();

    // 创建控件窗口
    HWND Create(HWND hParent, const RECT& rc, int controlId);

    // 公共接口
    void SetText(const std::wstring& text);
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }
    HWND GetHwnd() const { return m_hwnd; }

    // 初始化 Direct2D 资源（在父窗口创建后调用）
    void InitRenderResources();
    void DiscardRenderResources();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg,
                                     WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 消息处理
    void OnPaint();
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnSetFocus(HWND hwndOldFocus);
    void OnKillFocus(HWND hwndNewFocus);
    void OnKeyDown(WPARAM vk);
    void OnEnable(BOOL bEnable);
    LRESULT OnGetDlgCode(MSG* pMsg);

    // 状态推导
    ButtonState ComputeVisualState() const;

    // 绘制
    void DrawButton(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect);
    void CreateBrushes();

    // 鼠标跟踪
    void TrackMouseLeave();

private:
    HWND m_hwnd = nullptr;
    int  m_controlId = 0;
    bool m_enabled = true;
    bool m_hovering = false;
    bool m_pressed = false;
    bool m_focused = false;
    bool m_trackingMouse = false;
    std::wstring m_text = L"Button";

    // Direct2D 资源
    ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushNormal;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushHover;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushPressed;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushDisabled;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushBorder;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushText;
    ComPtr<ID2D1SolidColorBrush>  m_pBrushFocusRect;
    ComPtr<IDWriteTextFormat>     m_pTextFormat;

    static const wchar_t* s_className;
    static bool s_classRegistered;
};
```

这个类的结构很直白——公共接口提供创建、设置文本、启用/禁用等功能；私有部分包括窗口过程、消息处理函数、绘制函数和 Direct2D 资源。值得特别注意的是 `m_trackingMouse` 这个标志——它的用途我们马上就会讲到。

## 第三步——窗口注册与创建

完全自绘控件需要一个自己的窗口类。这和普通的子窗口控件（如 `BUTTON`、`EDIT`）在本质上没有区别，只是窗口过程由我们自己实现。

```cpp
// CustomButton.cpp
#include "CustomButton.h"
#include <string>

const wchar_t* CustomButton::s_className = L"CustomButtonClass";
bool CustomButton::s_classRegistered = false;

void CustomButton::RegisterWindowClass(HINSTANCE hInstance)
{
    if (s_classRegistered) return;

    WNDCLASSEX wcex = { 0 };
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = hInstance;
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;   // 我们自己处理背景
    wcex.lpszClassName = s_className;

    RegisterClassEx(&wcex);
    s_classRegistered = true;
}

HWND CustomButton::Create(HWND hParent, const RECT& rc, int controlId)
{
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);
    RegisterWindowClass(hInst);

    m_controlId = controlId;

    // CreateWindowEx 中 WS_EX_CONTROLPARENT 让这个窗口能参与 Tab 导航
    m_hwnd = CreateWindowEx(
        WS_EX_CONTROLPARENT,
        s_className,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        rc.left, rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        hParent,
        (HMENU)(INT_PTR)controlId,
        hInst,
        this                // 传递 this 指针
    );

    return m_hwnd;
}
```

这里有一个关键的细节——`CreateWindowEx` 的最后一个参数传了 `this` 指针。我们需要在 `WM_NCCREATE` 消息中把它取出来，绑定到窗口的用户数据上：

```cpp
LRESULT CALLBACK CustomButton::WndProc(HWND hwnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam)
{
    CustomButton* pThis = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        // 从 CREATESTRUCT 中取出 this 指针
        CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<CustomButton*>(pCS->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    }
    else
    {
        pThis = reinterpret_cast<CustomButton*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis)
        return pThis->HandleMessage(uMsg, wParam, lParam);

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这是一个非常经典的 Win32 面向对象封装模式——通过 `GWLP_USERDATA` 把 C++ 对象和窗口句柄关联起来，让静态的窗口过程能把消息分发到正确的对象方法上。你可能见过其他做法（比如用 `SetWindowLongPtr` + `GWLP_WNDPROC` 做子类化），但对于完全自绘的控件来说，注册自己的窗口类然后走 `GWLP_USERDATA` 这条路更干净。

## 第四步——TrackMouseEvent 与 Hover 检测

接下来是我们今天的重头戏之一——鼠标悬停（Hover）状态的检测。

Windows 默认不会发送 `WM_MOUSELEAVE` 消息。这一点很多初学者会搞错——他们以为只要处理了 `WM_MOUSEMOVE` 就够了，然后在 `WM_PAINT` 里用 `GetCursorPos` + `WindowFromPoint` 来判断鼠标是否在控件上。这种做法在大多数情况下能"碰巧"工作，但它有一个致命的问题：当鼠标离开控件区域时，控件不会收到任何通知，所以 Hover 状态永远不会被清除。结果就是控件卡在 Hover 状态不恢复——除非窗口恰好因为其他原因被重绘了。

正确的做法是使用 `TrackMouseEvent` 函数来注册鼠标离开通知。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackmouseevent)，`TrackMouseEvent` 函数接受一个 `TRACKMOUSEEVENT` 结构体，可以请求系统在鼠标离开窗口或悬停一段时间后发送通知。

而且有一个很重要的细节——`TrackMouseEvent` 注册的跟踪是**一次性**的。当系统发送 `WM_MOUSELEAVE` 消息后，跟踪自动取消。这意味着下次鼠标重新进入窗口时，我们必须再次调用 `TrackMouseEvent` 来注册新的跟踪。

```cpp
void CustomButton::TrackMouseLeave()
{
    if (m_trackingMouse) return;  // 已经在跟踪了，不需要重复注册

    TRACKMOUSEEVENT tme = { 0 };
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;      // 请求 WM_MOUSELEAVE 通知
    tme.hwndTrack = m_hwnd;
    tme.dwHoverTime = HOVER_DEFAULT;

    if (TrackMouseEvent(&tme))
    {
        m_trackingMouse = true;
    }
}
```

然后在 `WM_MOUSEMOVE` 处理中调用它：

```cpp
void CustomButton::OnMouseMove(int x, int y)
{
    if (!m_enabled) return;

    bool wasHovering = m_hovering;
    m_hovering = true;

    // 每次鼠标移动时注册离开跟踪
    // 这是必须的，因为 TME_LEAVE 跟踪是一次性的
    TrackMouseLeave();

    // 如果鼠标之前不在 Hover 状态，现在进入了，需要重绘
    if (!wasHovering)
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void CustomButton::OnMouseLeave()
{
    m_trackingMouse = false;  // 跟踪已自动取消
    m_hovering = false;
    m_pressed = false;        // 安全起见，也清除按下状态

    // 状态变了，需要重绘
    InvalidateRect(m_hwnd, nullptr, FALSE);
}
```

⚠️ 这里有一个经典坑点：很多开发者忘记在 `WM_MOUSELEAVE` 的处理中把 `m_trackingMouse` 重置为 `false`。后果是什么？下次鼠标再进入窗口时，`TrackMouseLeave()` 检查到 `m_trackingMouse` 已经是 `true`，就直接返回了——但此时系统已经不再跟踪了！所以控件永远收不到下一次 `WM_MOUSELEAVE`，又回到了 Hover 状态无法清除的老问题。

另外一个更隐蔽的坑是在 `WM_PAINT` 中用 `GetCursorPos` 来检测 Hover 状态。这种做法的问题在于：`WM_PAINT` 可能因为很多原因被触发（窗口被遮挡后显示、系统主题变化、其他窗口调用 `InvalidateRect` 等），在这些情况下鼠标根本没动过，但你基于 `GetCursorPos` 的判断却给出了一个 Hover/Not Hover 的结果。正确的做法是维护一个 `m_hovering` 布尔标志，只在鼠标事件中修改它，`WM_PAINT` 中只读取它。

## 第五步——完整的消息处理

有了 Hover 检测的基础，我们来看完整的消息处理框架。这里是控件交互逻辑的核心所在。

```cpp
LRESULT CustomButton::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSELEAVE:
        OnMouseLeave();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_SETFOCUS:
        OnSetFocus((HWND)wParam);
        return 0;

    case WM_KILLFOCUS:
        OnKillFocus((HWND)wParam);
        return 0;

    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;

    case WM_ENABLE:
        OnEnable((BOOL)wParam);
        return 0;

    case WM_GETDLGCODE:
        // 告诉对话框管理器：我要处理方向键和 Enter/Space
        return DLGC_WANTARROWS | DLGC_WANTCHARS |
               DLGC_BUTTON | DLGC_DEFPUSHBUTTON;

    case WM_ERASEBKGND:
        // 我们自己处理绘制，不需要系统擦除背景
        return 1;

    default:
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }
}
```

`WM_GETDLGCODE` 这个消息可能有些读者不熟悉。当控件在对话框中（或者父窗口调用了 `IsDialogMessage`）时，对话框管理器通过这个消息来询问控件想要处理哪些键盘输入。我们返回 `DLGC_WANTARROWS` 表示要处理方向键（否则 Tab 导航可能不正常），返回 `DLGC_DEFPUSHBUTTON` 表示这个按钮可以作为对话框的默认按钮响应 Enter 键。

接下来是鼠标按键和焦点事件的处理：

```cpp
void CustomButton::OnLButtonDown(int x, int y)
{
    if (!m_enabled) return;

    // 按下时捕获鼠标——这是为了在鼠标拖出控件范围后
    // 仍然能收到 WM_LBUTTONUP 来正确恢复状态
    SetCapture(m_hwnd);

    m_pressed = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CustomButton::OnLButtonUp(int x, int y)
{
    if (!m_enabled) return;

    bool wasPressed = m_pressed;
    m_pressed = false;

    // 释放鼠标捕获
    ReleaseCapture();

    // 只有在鼠标仍然在控件范围内且之前是按下状态时才触发点击
    if (wasPressed && m_hovering)
    {
        // 发送 WM_COMMAND 通知父窗口
        SendMessage(GetParent(m_hwnd), WM_COMMAND,
                    MAKELONG(m_controlId, BN_CLICKED),
                    (LPARAM)m_hwnd);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}
```

`OnLButtonDown` 中调用 `SetCapture` 是一个非常重要的操作。考虑这样一个场景：用户在按钮上按下鼠标左键，然后拖动鼠标移出了按钮的区域——此时按钮应该取消按下状态（否则用户松开鼠标后按钮还卡在 Pressed 状态）。`SetCapture` 让控件即使不在鼠标下方也能继续接收 `WM_MOUSEMOVE` 和 `WM_LBUTTONUP` 消息。在 `OnLButtonUp` 中我们可以检查 `m_hovering` 标志来决定是否要触发点击——只有鼠标在控件范围内松开才算有效点击。

关于鼠标捕获的更深入讨论（比如 Rubber Band 拖拽选择框的实现），我们会在下一篇文章中展开。

焦点处理相对来说简单一些：

```cpp
void CustomButton::OnSetFocus(HWND hwndOldFocus)
{
    m_focused = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CustomButton::OnKillFocus(HWND hwndNewFocus)
{
    m_focused = false;
    m_pressed = false;  // 安全起见
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CustomButton::OnKeyDown(WPARAM vk)
{
    if (!m_enabled) return;

    if (vk == VK_SPACE || vk == VK_RETURN)
    {
        // Space 和 Enter 模拟按钮按下
        m_pressed = true;
        InvalidateRect(m_hwnd, nullptr, FALSE);

        // 模拟一个"按下然后松开"的完整点击
        // 实际项目中这里可以用定时器做按下延迟
        m_pressed = false;

        SendMessage(GetParent(m_hwnd), WM_COMMAND,
                    MAKELONG(m_controlId, BN_CLICKED),
                    (LPARAM)m_hwnd);

        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void CustomButton::OnEnable(BOOL bEnable)
{
    m_enabled = (bEnable != FALSE);
    if (!m_enabled)
    {
        m_pressed = false;
        m_hovering = false;
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}
```

键盘处理中我们简单地在 Space/Enter 按下时直接触发点击事件。在实际产品中，你可能需要加入更多的细节——比如按下时先进入 Pressed 状态，松开时才触发；或者加入按下时的视觉反馈延迟。但对于框架演示来说这已经足够说明问题了。

## 第六步——Direct2D 绘制实现

消息处理的骨架搭好了，接下来是视觉呈现的部分。我们用 Direct2D 来绘制按钮的各个状态。

首先初始化渲染资源。Direct2D 的工厂和渲染目标创建是一个比较模板化的过程——先创建工厂，再通过工厂创建 `HwndRenderTarget`：

```cpp
void CustomButton::InitRenderResources()
{
    if (m_pRenderTarget) return;  // 已经初始化过了

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(
        rc.right - rc.left,
        rc.bottom - rc.top
    );

    // 创建 Direct2D 工厂（如果还没有的话）
    // 实际项目中建议把工厂做成全局/父窗口级别的共享资源
    ComPtr<ID2D1Factory> pFactory;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREAD, &pFactory);

    // 创建 HwndRenderTarget——绑定到控件窗口的渲染目标
    pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pRenderTarget
    );

    CreateBrushes();

    // 创建文字格式
    ComPtr<IDWriteFactory> pDWriteFactory;
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(pDWriteFactory.GetAddressOf())
    );

    pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"zh-CN",
        &m_pTextFormat
    );

    m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void CustomButton::CreateBrushes()
{
    if (!m_pRenderTarget) return;

    // 各状态对应的背景色
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), &m_pBrushNormal);       // 白色
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::LightGray), &m_pBrushHover);    // 浅灰
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::SkyBlue), &m_pBrushPressed);    // 天蓝
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f)),
        &m_pBrushDisabled);                                          // 极浅灰
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::DarkGray), &m_pBrushBorder);    // 边框灰色
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &m_pBrushText);         // 文字黑色
    m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::CornflowerBlue), &m_pBrushFocusRect);
}
```

这里有一个值得讨论的设计决策：每个控件实例都创建自己的 `ID2D1Factory` 和 `ID2D1HwndRenderTarget`。在演示代码中这样做可以简化逻辑，但在实际项目中这会浪费资源——`ID2D1Factory` 应该是全局单例或至少是父窗口级别的共享资源，画刷也可以用共享的方式管理。不过对于理解自绘控件的架构来说，这个简化是合理的。

绘制函数本身的逻辑很清晰——根据当前状态选择对应的画刷，然后画圆角矩形背景、边框、文字，以及焦点指示器：

```cpp
void CustomButton::OnPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(m_hwnd, &ps);

    InitRenderResources();

    if (!m_pRenderTarget)
    {
        EndPaint(m_hwnd, &ps);
        return;
    }

    // 确保 RenderTarget 尺寸与窗口匹配
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

    if (m_pRenderTarget->GetPixelSize() != size)
    {
        m_pRenderTarget->Resize(size);
    }

    // 检查 RenderTarget 是否可用
    if (m_pRenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)
    {
        EndPaint(m_hwnd, &ps);
        return;
    }

    m_pRenderTarget->BeginDraw();

    // 清除背景
    m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 绘制按钮
    D2D1_RECT_F rect = D2D1::RectF(
        0.5f, 0.5f,
        static_cast<float>(rc.right) - 0.5f,
        static_cast<float>(rc.bottom) - 0.5f
    );

    DrawButton(m_pRenderTarget.Get(), rect);

    HRESULT hr = m_pRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardRenderResources();
    }

    EndPaint(m_hwnd, &ps);
}

void CustomButton::DrawButton(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect)
{
    ButtonState state = ComputeVisualState();

    // 选择背景画刷
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    switch (state)
    {
    case ButtonState::Normal:   pBgBrush = m_pBrushNormal.Get();   break;
    case ButtonState::Hover:    pBgBrush = m_pBrushHover.Get();    break;
    case ButtonState::Pressed:  pBgBrush = m_pBrushPressed.Get();  break;
    case ButtonState::Disabled: pBgBrush = m_pBrushDisabled.Get(); break;
    case ButtonState::Focused:  pBgBrush = m_pBrushNormal.Get();   break;
    }

    // 绘制圆角矩形背景
    float radiusX = 4.0f, radiusY = 4.0f;
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(rect, radiusX, radiusY);

    if (pBgBrush)
    {
        pRT->FillRoundedRectangle(roundedRect, pBgBrush);
    }

    // 绘制边框
    pRT->DrawRoundedRectangle(roundedRect, m_pBrushBorder.Get(), 1.0f);

    // 绘制焦点指示器（虚线矩形）
    if (state == ButtonState::Focused)
    {
        D2D1_RECT_F focusRect = D2D1::RectF(
            rect.left + 3.0f, rect.top + 3.0f,
            rect.right - 3.0f, rect.bottom - 3.0f
        );

        // 创建虚线描边样式
        // 注意：实际项目中应该缓存这个样式，不要每次绘制都创建
        ComPtr<ID2D1Factory> pFactory;
        m_pRenderTarget->GetFactory(&pFactory);

        float dashes[] = { 2.0f, 2.0f };
        ComPtr<ID2D1StrokeStyle> pStrokeStyle;
        pFactory->CreateStrokeStyle(
            D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_FLAT,
                D2D1_CAP_STYLE_FLAT,
                D2D1_CAP_STYLE_FLAT,
                D2D1_LINE_JOIN_MITER,
                10.0f,
                D2D1_DASH_STYLE_CUSTOM,
                0.0f),
            dashes, 2, &pStrokeStyle
        );

        pRT->DrawRectangle(focusRect, m_pBrushFocusRect.Get(),
                           1.0f, pStrokeStyle.Get());
    }

    // 绘制文字
    if (m_pTextFormat && m_pBrushText)
    {
        pRT->DrawText(
            m_text.c_str(),
            static_cast<UINT32>(m_text.length()),
            m_pTextFormat.Get(),
            rect,
            state == ButtonState::Disabled
                ? m_pBrushBorder.Get()   // 禁用时文字变灰
                : m_pBrushText.Get()
        );
    }
}

void CustomButton::DiscardRenderResources()
{
    m_pRenderTarget.Reset();
    m_pBrushNormal.Reset();
    m_pBrushHover.Reset();
    m_pBrushPressed.Reset();
    m_pBrushDisabled.Reset();
    m_pBrushBorder.Reset();
    m_pBrushText.Reset();
    m_pBrushFocusRect.Reset();
    m_pTextFormat.Reset();
}
```

注意 `DrawButton` 中焦点指示器的虚线矩形——这是 Windows 无障碍规范的视觉要求。标准控件的焦点指示器就是这样一个虚线矩形（或点线矩形），我们的自绘控件也应该遵循这个惯例。

另外，`EndDraw` 返回 `D2DERR_RECREATE_TARGET` 的情况需要处理。这通常发生在显卡驱动重置或设备丢失的场景（比如电脑睡眠唤醒后），此时我们需要丢弃所有渲染资源并在下一次绘制时重新创建。

## 第七步——集成到父窗口

最后我们来看看如何在父窗口中使用这个自绘按钮。整个过程和创建普通子窗口控件差不多，只是多了一步渲染资源的初始化：

```cpp
// 主窗口过程片段
#define IDC_CUSTOM_BTN  1001

CustomButton g_btnOk;
CustomButton g_btnCancel;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建两个自绘按钮
        g_btnOk.Create(hwnd, { 50, 200, 200, 240 }, IDC_CUSTOM_BTN);
        g_btnOk.SetText(L"OK");

        g_btnCancel.Create(hwnd, { 220, 200, 370, 240 }, IDC_CUSTOM_BTN + 1);
        g_btnCancel.SetText(L"Cancel");

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        // 父窗口的绘制逻辑...
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
    {
        int ctrlId = LOWORD(wParam);
        switch (ctrlId)
        {
        case IDC_CUSTOM_BTN:
            MessageBox(hwnd, L"OK clicked!", L"Info", MB_OK);
            break;
        case IDC_CUSTOM_BTN + 1:
            MessageBox(hwnd, L"Cancel clicked!", L"Info", MB_OK);
            break;
        }
        return 0;
    }

    case WM_SIZE:
    {
        // 窗口大小变化时重新布局控件
        int cx = LOWORD(lParam);
        g_btnOk.Create(/* 新位置 */);  // 或者用 MoveWindow
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

⚠️ 这里有一个需要注意的地方：如果你在父窗口中处理了 `IsDialogMessage`（用于实现 Tab 键导航），那么自绘按钮的 `WS_TABSTOP` 样式才能生效。纯 Win32 程序的消息循环通常是这样的：

```cpp
// 消息循环中启用 Tab 导航
MSG msg;
while (GetMessage(&msg, nullptr, 0, 0))
{
    if (!IsDialogMessage(hMainWnd, &msg))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

`IsDialogMessage` 会在内部处理 Tab、Shift+Tab、方向键等导航按键，如果消息是导航相关的就不返回给我们的消息循环。这也就是为什么我们之前在 `WM_GETDLGCODE` 中返回了那些标志——告诉 `IsDialogMessage` 我们的控件需要参与导航逻辑。

## ⚠️ 踩坑预警

在实现完全自绘控件的过程中，有几个坑点特别值得注意。

第一个坑我们已经讨论过了——`TrackMouseEvent` 的一次性问题。忘记在 `WM_MOUSELEAVE` 后重置跟踪标志，或者忘记在鼠标重新进入时重新注册跟踪，都会导致控件卡在 Hover 状态。

第二个坑是 `WM_MOUSEMOVE` 的频率问题。Windows 会在鼠标移动时持续发送 `WM_MOUSEMOVE` 消息，如果每次都调用 `InvalidateRect` 触发重绘，在高 DPI 屏幕上可能会造成性能问题。正确的做法是只在状态实际发生变化时才触发重绘——也就是我们代码中那个 `if (!wasHovering)` 的判断。

第三个坑是 `WM_ENABLE` 消息的触发方式。`EnableWindow(hwnd, FALSE)` 会发送 `WM_ENABLE` 消息给控件，但如果控件是被父窗口禁用的（父窗口被 `EnableWindow(hParent, FALSE)`），控件不会直接收到 `WM_ENABLE`——它会被隐式禁用。如果你的控件需要在被隐式禁用时也更新视觉状态，你可能需要处理 `WM_ENABLE` 的同时也在父窗口的通知中做处理。

第四个坑是 Direct2D 资源的设备丢失恢复。在显卡驱动崩溃恢复、电脑睡眠唤醒等场景下，Direct2D 的渲染目标会失效。我们的代码中通过检查 `EndDraw` 的返回值来处理了这个问题，但如果你在其他地方（比如动画定时器回调中）也使用了渲染资源，也需要做同样的检查。

## 常见问题

**Q: 自绘控件和 Owner-Draw 控件有什么本质区别？**

A: Owner-Draw 控件（使用 `WM_DRAWITEM`）本质上是"系统管行为、我们管外观"。控件的窗口类仍然是系统预定义的（如 `BUTTON`），系统负责处理鼠标、键盘、焦点等交互逻辑，我们只在需要绘制的时候被通知。而完全自绘控件则是我们自己注册窗口类、自己处理所有消息——行为和外观都由我们完全控制。前者适合在系统控件基础上做外观定制，后者适合创建全新的交互控件。

**Q: 为什么不用 GDI+ 而用 Direct2D？**

A: GDI+ 虽然也能做到抗锯齿和 Alpha 混合，但它是纯软件渲染，在高频率重绘场景下性能不足。Direct2D 底层调用 Direct3D，享有 GPU 硬件加速。对于需要动画效果的自绘控件来说，Direct2D 的性能优势是决定性的。

**Q: 自绘控件如何支持无障碍（Accessibility）？**

A: 完整的无障碍支持需要实现 `IAccessible` 或 UI Automation（`IRawElementProviderSimple`）接口。这包括提供控件的角色（Role）、状态（State）、名称（Name）、值（Value）等信息。实现起来比较复杂，但如果你想让自绘控件能被屏幕阅读器识别、能通过辅助技术操作，这是必须要做的。我们今天只实现了视觉焦点指示器，但框架已经为后续扩展无障碍接口留出了空间。

**Q: 多个自绘控件之间如何共享 Direct2D 资源？**

A: `ID2D1Factory` 是线程安全的（多线程版本），可以在整个应用中共享。`ID2D1HwndRenderTarget` 不能共享——每个控件窗口需要自己的渲染目标。但画刷（`ID2D1SolidColorBrush` 等）可以在同一个渲染目标创建后跨控件使用，前提是渲染目标兼容。实际项目中推荐的做法是创建一个"渲染上下文"类来管理所有共享资源。

## 总结

到这里，我们已经完成了一个完全自绘 Button 控件的完整框架。从状态机的定义到 TrackMouseEvent 的 Hover 检测，从鼠标/键盘消息处理到 Direct2D 绘制，我们一步步拆解了自绘控件的每一个环节。

核心要点可以归纳为：用推导式状态计算代替手动状态维护、用 TrackMouseEvent 正确管理 Hover 状态、用 SetCapture/ReleaseCapture 处理鼠标按下拖动的场景、以及用 Direct2D 做硬件加速的高质量绘制。

这个框架已经是一个可工作的自绘控件了，但它还有很大的扩展空间——比如加入按下时的缩放动画、Hover 时的渐变过渡、或者是多状态之间的平滑颜色插值。这些动画效果的实现需要引入定时器和插值计算，属于我们今天留给大家练习的方向。

下一篇我们将深入自绘控件的另一个核心话题——命中测试与鼠标事件路由。当控件不再是简单的矩形，当多个控件发生重叠时，如何精确判断鼠标点击了哪个控件？`PtInRect` 显然不够用了，我们需要 Direct2D Geometry 的精确命中测试，以及鼠标捕获机制在 Rubber Band 拖拽选择中的应用。

---

## 练习

1. **基础练习——状态颜色定制**：修改按钮的各状态颜色，让它看起来更像一个现代的 Flat Design 按钮（Normal 状态透明背景、Hover 状态浅色背景、Pressed 状态深色背景）。

2. **进阶练习——按下动画**：为按钮添加按下时的缩放动画效果——当鼠标按下时按钮缩小到 95% 大小，松开时恢复到 100%。提示：使用 `SetTimer` + `KillTimer`，在定时器回调中逐帧更新缩放比例。

3. **实战练习——Toggle 开关控件**：基于本文的自绘控件框架，实现一个 Toggle 开关控件。要求包含滑块从左到右（或从右到左）的平滑过渡动画。提示：维护一个 0.0-1.0 的动画进度值（0 表示关、1 表示开），用定时器驱动进度值的变化。

4. **思考题——无障碍扩展**：如果你要为这个自绘按钮添加 UI Automation 支持，需要实现哪些接口？最少需要提供哪些属性（Name、Role、State 等）才能让屏幕阅读器正确识别这个控件？查阅 Microsoft Learn 上关于 `IRawElementProviderSimple` 的文档，给出你的设计方案。

---

**参考资料**:
- [TrackMouseEvent function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackmouseevent)
- [TRACKMOUSEEVENT structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-trackmouseevent)
- [WM_MOUSELEAVE message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseleave)
- [SetCapture function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)
- [ReleaseCapture function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-releasecapture)
- [WM_GETDLGCODE message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/dlgbox/wm-getdlgcode)
- [Direct2D Geometries Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-geometries-overview)
- [ID2D1HwndRenderTarget - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1hwndrendertarget)
