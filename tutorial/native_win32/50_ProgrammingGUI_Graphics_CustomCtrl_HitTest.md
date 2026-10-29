# 通用GUI编程技术——图形渲染实战（五十）——命中测试与鼠标事件路由：精确交互

> 上一篇文章我们搭建了完全自绘控件的完整框架——状态机、Hover 检测、键盘导航、Direct2D 绘制，一应俱全。但有一个环节我们只是简单带过了：命中测试。在按钮那个例子中，命中测试几乎是"免费的"——因为按钮是矩形，鼠标是否在按钮上只需要一个简单的坐标范围判断。但现实世界远比矩形复杂。当你需要点击一个圆形按钮、选择一条自由绘制的曲线、或者在一个包含几十个重叠对象的画布上精确点选某一个元素时，简单的矩形判断就完全不够用了。

今天这篇文章我们就来深入命中测试和鼠标事件路由这两个紧密关联的话题。我们会从最基本的 `PtInRect` 出发，一路讲到 Direct2D Geometry 的精确命中测试，然后再用 `SetCapture`/`ReleaseCapture` 实现一个完整的 Rubber Band 拖拽选择框——这是几乎所有绘图应用都会用到的基础交互。

## 环境说明

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **图形API**: Direct2D + GDI（本章同时使用两种 API 做对比）
- **目标平台**: Win32 API 原生开发

## 从最简单的命中测试说起

如果你之前做过任何 GUI 编程，`PtInRect` 大概是你接触到的第一个命中测试函数。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-ptinrect)，它的函数签名是：

```cpp
BOOL PtInRect(const RECT *lprc, POINT pt);
```

用法极其简单——传入一个矩形和一个点，返回 TRUE 或 FALSE。但这里有一个容易忽略的细节：`PtInRect` 对矩形边界的判定是"左闭右开"的——也就是说，点在矩形的左边和上边时返回 TRUE，但在右边和下边时返回 FALSE。这个设计是为了和 Windows 的 RECT 约定一致（right 和 bottom 是不包含在矩形内的）。

对于矩形控件来说，`PtInRect` 已经完全够用了。我们的自绘按钮只需要在收到鼠标消息时检查鼠标坐标是否在控件的客户区域内：

```cpp
bool IsPointInButton(POINT pt, const RECT& buttonRect)
{
    return PtInRect(&buttonRect, pt) != FALSE;
}
```

但当我们需要处理非矩形形状时，事情就开始变得复杂了。

## 矩形 vs 精确几何命中测试

考虑一个场景：你在一个画布上画了一个圆形、一个星形、一条贝塞尔曲线，用户想点击选中其中一个。用矩形包围盒（Bounding Box）来做命中测试会导致严重的误判——圆的包围盒是正方形，星形的包围盒也是矩形，大量"空白区域"都会被错误地命中。

### Direct2D Geometry 的精确命中测试

Direct2D 提供了 `ID2D1Geometry` 接口来做精确的几何命中测试。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1geometry)，`ID2D1Geometry` 有两个核心的命中测试方法：

`FillContainsPoint` 检查一个点是否在几何体的填充区域内部，`StrokeContainsPoint` 检查一个点是否在几何体的描边（轮廓线）上。后者特别适用于线条和路径的选择。

先来看 `FillContainsPoint` 的使用方式：

```cpp
bool HitTestGeometry(ID2D1Geometry* pGeometry, D2D1_POINT_2F point)
{
    BOOL contains = FALSE;
    HRESULT hr = pGeometry->FillContainsPoint(
        point,                   // 要测试的点
        nullptr,                 // 世界变换矩阵（可选，传 nullptr 表示单位矩阵）
        &contains                // 输出结果
    );
    return SUCCEEDED(hr) && contains;
}
```

这个函数非常直观——给定一个几何体和一个点，告诉你点是否在几何体内部。对于圆形、椭圆、多边形、甚至复杂的路径几何体，它都能给出精确的结果。Direct2D 在内部会使用数学方法（而不是像素级判断）来计算，所以精度非常高，性能也远优于"先渲染到位图再逐像素判断"的笨办法。

接下来是一个实际的例子——假设我们的画布上有多个图形对象，每个对象都有自己的 Geometry，用户点击时我们需要找到被点击的那个：

```cpp
struct Shape
{
    ComPtr<ID2D1Geometry> geometry;    // 几何形状
    D2D1::ColorF fillColor;            // 填充颜色
    D2D1::ColorF strokeColor;          // 描边颜色
    float strokeWidth;                 // 描边宽度
    bool selected = false;             // 是否被选中
};

// 在画布的形状列表中查找被点击的形状（从上到下遍历，返回最上面的）
Shape* HitTestShapes(const std::vector<Shape>& shapes,
                     D2D1_POINT_2F point,
                     float strokeTolerance = 1.0f)
{
    // 从后往前遍历——后面的形状画在最上面，所以优先级更高
    for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i)
    {
        BOOL contains = FALSE;

        // 先检查填充区域
        shapes[i].geometry->FillContainsPoint(point, nullptr, &contains);
        if (contains) return const_cast<Shape*>(&shapes[i]);

        // 如果不在填充区域内，再检查描边
        // strokeWidth / 2 是因为描边是以路径为中心向两侧扩展的
        BOOL onStroke = FALSE;
        shapes[i].geometry->StrokeContainsPoint(
            point,
            shapes[i].strokeWidth,
            nullptr,    // stroke style（可选）
            nullptr,    // 世界变换矩阵
            strokeTolerance,
            &onStroke
        );
        if (onStroke) return const_cast<Shape*>(&shapes[i]);
    }

    return nullptr;  // 没有命中任何形状
}
```

这段代码的遍历顺序很重要——我们从 `shapes` 数组的末尾开始往前遍历，因为后面绘制的形状在视觉上会覆盖前面的形状。当你点击一个重叠区域时，应该选中最上面的那个。这就是所谓的"Z 序"——绘制顺序决定了选择优先级。

### 命中测试的容差

你可能注意到 `StrokeContainsPoint` 的最后一个参数 `strokeTolerance`。这个参数叫做"展平容差"（Flattening Tolerance），它控制几何体在命中测试时的精度。默认值通常是 `D2D1_DEFAULT_FLATTENING_TOLERANCE`（约 0.25 像素）。

在实际应用中，用户的手指或鼠标光标不可能精确到亚像素级别，所以给命中测试加一点"容差"是很有必要的。特别是对于线条（描边宽度很小的路径），如果没有足够的容差，用户几乎不可能精确点击到线条上。一个常见的做法是将描边宽度加上一个固定的容差值（比如 4-6 像素），这样即使鼠标稍微偏离线条也能命中。

```cpp
// 带容差的描边命中测试
bool HitTestStroke(ID2D1Geometry* pGeometry,
                   D2D1_POINT_2F point,
                   float strokeWidth,
                   float extraTolerance = 4.0f)
{
    BOOL onStroke = FALSE;
    pGeometry->StrokeContainsPoint(
        point,
        strokeWidth + extraTolerance * 2.0f,  // 扩大描边宽度来增加容差
        nullptr, nullptr,
        D2D1_DEFAULT_FLATTENING_TOLERANCE,
        &onStroke
    );
    return onStroke != FALSE;
}
```

## SetCapture 与 ReleaseCapture：鼠标捕获机制

命中测试解决了"点击了哪里"的问题，但鼠标交互还有另一个维度——"拖拽过程中鼠标跑出窗口怎么办"。

考虑一个拖拽场景：用户在画布上按下鼠标左键，然后拖动鼠标来移动一个图形。如果用户拖动得太快，鼠标可能会跑出窗口区域——此时窗口不再接收 `WM_MOUSEMOVE` 消息，拖拽就"断"了。更糟糕的是，鼠标在窗口外面松开后，窗口收不到 `WM_LBUTTONUP`，控件会一直卡在"按下"状态。

`SetCapture` 和 `ReleaseCapture` 就是 Windows 为这个问题提供的解决方案。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)，`SetCapture` 的作用是将所有后续的鼠标输入事件路由到指定的窗口，无论鼠标光标是否在该窗口的范围内。

```cpp
// SetCapture 签名
HWND SetCapture(HWND hWnd);

// ReleaseCapture 签名
BOOL ReleaseCapture();
```

调用 `SetCapture` 后，目标窗口会收到所有鼠标消息，就像鼠标始终在窗口内一样。鼠标坐标可能超出窗口范围（甚至是负数或大于窗口尺寸），但消息仍然会被发送到捕获窗口。这在拖拽操作中至关重要。

不过需要注意一个限制：`SetCapture` 只能捕获同一个线程中的鼠标消息。如果用户切换到了另一个线程的窗口，或者系统弹出了一个模态对话框，鼠标捕获会被自动释放（你会收到 `WM_CAPTURECHANGED` 消息）。

### 鼠标捕获的最佳实践

关于 `SetCapture`/`ReleaseCapture` 的使用，有几个重要的最佳实践：

首先，`SetCapture` 必须和 `ReleaseCapture` 配对使用。如果在 `SetCapture` 后忘记调用 `ReleaseCapture`，整个系统的鼠标交互都会被你的窗口"吞掉"——其他窗口无法接收鼠标点击。这对于用户来说是灾难性的体验。

其次，`ReleaseCapture` 的调用时机应该是"鼠标操作完成时"——对于点击来说是 `WM_LBUTTONUP`，对于拖拽来说是拖拽结束。如果在某些异常情况下（比如窗口被销毁）需要提前释放捕获，可以在 `WM_DESTROY` 中做兜底处理。

最后，如果你的应用中有多个窗口可能同时想捕获鼠标，需要注意 Windows 只允许一个窗口拥有捕获。调用 `SetCapture` 会自动从之前的窗口夺走捕获权，所以要注意协调。

```cpp
LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMsg,
                                WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    {
        // 开始拖拽操作
        SetCapture(hwnd);
        g_dragging = true;
        g_dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (g_dragging)
        {
            // 处理拖拽中的逻辑
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            // 注意：坐标可能超出窗口范围！
            // 需要做边界检查
            UpdateDrag(x, y);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_dragging)
        {
            ReleaseCapture();
            g_dragging = false;
            FinishDrag();
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
    {
        // 捕获被系统强制夺走（比如用户切换了窗口）
        if (g_dragging)
        {
            g_dragging = false;
            CancelDrag();
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

`WM_CAPTURECHANGED` 消息是一个很重要的安全网。当系统因为某些原因（用户按了 Alt+Tab、系统弹出了 Task Manager 等）强制释放了你的鼠标捕获时，你会收到这个消息。如果你不在此时清理拖拽状态，控件会卡在一个奇怪的中间状态。

## 实战：Rubber Band 拖拽选择框

现在我们把上面学到的所有知识综合起来，实现一个经典的交互功能——Rubber Band 选择框。这是几乎所有绘图软件（Photoshop、Visio、Figma……）都支持的操作：用户按住鼠标左键拖拽出一个矩形选框，松开后选框内的所有对象被选中。

### 数据结构定义

```cpp
// RubberBandSelector.h
#pragma once
#include <windows.h>
#include <vector>
#include <algorithm>

struct DrawShape
{
    RECT bounds;          // 形状的包围盒
    int  id;              // 唯一标识
    bool selected = false;
};

class RubberBandSelector
{
public:
    RubberBandSelector(HWND hCanvas);

    // 添加形状
    int  AddShape(const RECT& bounds);
    void ClearShapes();

    // 获取选中的形状
    const std::vector<int>& GetSelectedIds() const { return m_selectedIds; }

    // 选中指定范围内的形状
    void SelectInRect(const RECT& selectionRect);

    // 清除所有选中状态
    void ClearSelection();

    // 获取当前选框（拖拽过程中）
    bool IsSelecting() const { return m_selecting; }
    RECT GetSelectionRect() const { return m_selectionRect; }

    // 绘制选框
    void DrawRubberBand(HDC hdc) const;

private:
    HWND m_hCanvas;
    std::vector<DrawShape> m_shapes;
    std::vector<int> m_selectedIds;

    bool m_selecting = false;
    POINT m_startPt = { 0, 0 };
    POINT m_currentPt = { 0, 0 };
    RECT m_selectionRect = { 0 };

    void UpdateSelectionRect();
};
```

### 完整的拖拽选择实现

```cpp
// RubberBandSelector.cpp
#include "RubberBandSelector.h"

RubberBandSelector::RubberBandSelector(HWND hCanvas)
    : m_hCanvas(hCanvas)
{
}

int RubberBandSelector::AddShape(const RECT& bounds)
{
    static int nextId = 1;
    DrawShape shape;
    shape.bounds = bounds;
    shape.id = nextId++;
    shape.selected = false;
    m_shapes.push_back(shape);
    return shape.id;
}

void RubberBandSelector::ClearShapes()
{
    m_shapes.clear();
    m_selectedIds.clear();
}

void RubberBandSelector::ClearSelection()
{
    for (auto& shape : m_shapes)
        shape.selected = false;
    m_selectedIds.clear();
}

void RubberBandSelector::UpdateSelectionRect()
{
    // 确保 left < right, top < bottom（用户可能从右下往左上拖拽）
    m_selectionRect.left   = (std::min)(m_startPt.x, m_currentPt.x);
    m_selectionRect.top    = (std::min)(m_startPt.y, m_currentPt.y);
    m_selectionRect.right  = (std::max)(m_startPt.x, m_currentPt.x);
    m_selectionRect.bottom = (std::max)(m_startPt.y, m_currentPt.y);
}

void RubberBandSelector::SelectInRect(const RECT& selRect)
{
    m_selectedIds.clear();

    for (auto& shape : m_shapes)
    {
        // 检查形状的包围盒是否与选框相交
        // 如果包围盒完全在选框内，则选中
        if (selRect.left <= shape.bounds.left &&
            selRect.top <= shape.bounds.top &&
            selRect.right >= shape.bounds.right &&
            selRect.bottom >= shape.bounds.bottom)
        {
            shape.selected = true;
            m_selectedIds.push_back(shape.id);
        }
        else
        {
            shape.selected = false;
        }
    }
}

void RubberBandSelector::DrawRubberBand(HDC hdc) const
{
    if (!m_selecting) return;

    // 创建虚线画笔——这是 Rubber Band 选框的标志性视觉
    HPEN hPen = CreatePen(PS_DOT, 1, RGB(0, 0, 255));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // 选框内部用半透明蓝色填充
    // GDI 不直接支持半透明，所以用画刷图案模拟
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    // 设置混合模式——让选框背景反色显示
    int oldROP = SetROP2(hdc, R2_NOTXORPEN);

    Rectangle(hdc,
        m_selectionRect.left, m_selectionRect.top,
        m_selectionRect.right, m_selectionRect.bottom);

    SetROP2(hdc, oldROP);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
```

这里有一个绘制技巧值得一提——我们使用了 `R2_NOTXORPEN` 光栅操作码来绘制选框。这个操作码的效果是"异或"——在同一位置绘制两次会恢复原来的像素颜色。这就是所谓的"XOR 绘制"技术，它在实现拖拽预览时非常有用，因为你可以通过再次绘制来"擦除"上一帧的选框，而不需要重绘整个画面。不过在双缓冲场景下，XOR 技术的意义有所下降（因为我们总是先清除再重绘），但它仍然是 Rubber Band 选框的经典实现方式。

### 消息处理集成

现在我们来看完整的窗口过程，把 Rubber Band 选择和鼠标捕获整合在一起：

```cpp
// 画布窗口过程
RubberBandSelector* g_pSelector = nullptr;

LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMsg,
                                WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        g_pSelector = new RubberBandSelector(hwnd);

        // 添加一些示例形状
        g_pSelector->AddShape({ 50,  50,  150, 120 });
        g_pSelector->AddShape({ 200, 80,  320, 180 });
        g_pSelector->AddShape({ 100, 200, 250, 300 });
        g_pSelector->AddShape({ 300, 150, 450, 280 });
        g_pSelector->AddShape({ 180, 320, 350, 400 });
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 清除之前的选中状态
        g_pSelector->ClearSelection();

        // 开始 Rubber Band 选择
        SetCapture(hwnd);  // 捕获鼠标——拖出窗口也能继续跟踪

        g_pSelector->m_selecting = true;
        g_pSelector->m_startPt = { GET_X_LPARAM(lParam),
                                    GET_Y_LPARAM(lParam) };
        g_pSelector->m_currentPt = g_pSelector->m_startPt;
        g_pSelector->UpdateSelectionRect();

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (g_pSelector->m_selecting)
        {
            g_pSelector->m_currentPt = { GET_X_LPARAM(lParam),
                                          GET_Y_LPARAM(lParam) };
            g_pSelector->UpdateSelectionRect();

            // 实时预览选中的形状（可选）
            g_pSelector->SelectInRect(g_pSelector->GetSelectionRect());

            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_pSelector->m_selecting)
        {
            g_pSelector->m_selecting = false;

            // 最终确认选中
            g_pSelector->SelectInRect(g_pSelector->GetSelectionRect());

            ReleaseCapture();  // 释放鼠标捕获
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
    {
        // 捕获被意外释放——取消选择操作
        if (g_pSelector && g_pSelector->IsSelecting())
        {
            g_pSelector->m_selecting = false;
            g_pSelector->ClearSelection();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 双缓冲
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc,
            rcClient.right, rcClient.bottom);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

        // 白色背景
        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdcMem, &rcClient, hBg);
        DeleteObject(hBg);

        // 绘制形状
        DrawAllShapes(hdcMem);

        // 绘制 Rubber Band 选框
        g_pSelector->DrawRubberBand(hdcMem);

        // 一次性拷贝到屏幕
        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom,
               hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbm);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        delete g_pSelector;
        g_pSelector = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这段代码的流程很清晰：`WM_LBUTTONDOWN` 开始选择并捕获鼠标，`WM_MOUSEMOVE` 更新选框位置，`WM_LBUTTONUP` 结束选择并释放捕获。三个消息构成了一个完整的拖拽生命周期。

## WS_EX_TRANSPARENT 与事件穿透

在讨论鼠标事件路由时，有一个容易被误解的窗口扩展样式需要特别说明——`WS_EX_TRANSPARENT`。

很多开发者看到这个名字就以为它是让窗口"透明到鼠标点击"，让点击事件穿透到下面的窗口。但事实完全不是这样。根据 Raymond Chen 在 [Microsoft DevBlogs 上的经典解释](https://devblogs.microsoft.com/oldnewthing/20121217-00/?p=5823)，`WS_EX_TRANSPARENT` 的真正含义是影响**绘制顺序**——它让兄弟窗口（sibling windows）中位于当前窗口下面的窗口先被绘制，然后再绘制当前窗口。

如果你想让一个窗口真的对鼠标点击透明（即点击穿透），正确的做法是处理 `WM_NCHITTEST` 消息并返回 `HTTRANSPARENT`：

```cpp
case WM_NCHITTEST:
{
    // 让鼠标点击穿透到下方的窗口
    LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
    if (hit == HTCLIENT)
        return HTTRANSPARENT;
    return hit;
}
```

根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)，当 `WM_NCHITTEST` 返回 `HTTRANSPARENT` 时，系统会继续向 Z 序中下方的窗口发送 `WM_NCHITTEST` 消息，直到找到一个愿意接受这个点击的窗口。这就是事件穿透的正确实现方式。

这种技术在实际中有一些应用场景——比如覆盖在视频播放器上的半透明控制面板（在不点击按钮时让鼠标事件穿透到视频窗口）、或者游戏中的半透明 HUD 元素。

## Z 序与命中测试优先级

当一个窗口中有多个重叠的子窗口时，`WM_NCHITTEST` 消息的发送顺序遵循 Z 序——从最上面的窗口开始，依次往下。如果一个窗口返回 `HTTRANSPARENT`，系统会继续询问下一个窗口。如果所有窗口都返回 `HTTRANSPARENT`，消息最终会到达父窗口。

对于自绘控件来说，理解 Z 序对命中测试的影响很重要。如果你在父窗口上放置了多个自绘子控件，它们之间的重叠区域由 Z 序决定谁优先接收鼠标消息。你可以通过 `SetWindowPos` 的 `HWND_TOP`/`HWND_BOTTOM` 参数来调整 Z 序：

```cpp
// 将控件提升到 Z 序最上面（视觉上也显示在最上面）
SetWindowPos(hControl, HWND_TOP, 0, 0, 0, 0,
             SWP_NOMOVE | SWP_NOSIZE);
```

在我们的 Rubber Band 选择场景中，Z 序同样决定了形状的命中测试优先级——这就是为什么之前 `HitTestShapes` 函数从后往前遍历形状列表的原因。

## 鼠标采样不足与插值

在实现拖拽绘制或签名等功能时，你会发现一个令人头疼的问题：当鼠标快速移动时，`WM_MOUSEMOVE` 消息之间的间距会变得很大，导致绘制出来的线条出现明显的"直线段"而不是平滑的曲线。

这是因为 `WM_MOUSEMOVE` 消息不是"每一帧"都发送的——系统在合并连续的鼠标移动时会丢弃一些中间位置。当鼠标移动得非常快时，两个相邻 `WM_MOUSEMOVE` 消息之间的像素距离可能达到几十甚至上百像素。

解决这个问题的思路有两种。

第一种是提高采样率——使用 `GetMouseMovePointsEx` 函数来获取更精细的鼠标轨迹。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmousemovepointsex)，这个函数可以获取系统记录的最近 64 个鼠标位置点，分辨率远高于 `WM_MOUSEMOVE`：

```cpp
// 获取更精细的鼠标轨迹
void GetHighResolutionMousePath(POINT currentPt,
                                std::vector<POINT>& outPoints)
{
    MOUSEMOVEPOINT input = { 0 };
    input.x = currentPt.x;
    input.y = currentPt.y;
    input.time = GetTickCount();

    MOUSEMOVEPOINT points[64] = { 0 };
    int count = GetMouseMovePointsEx(
        sizeof(MOUSEMOVEPOINT),
        &input,
        points,
        64,
        GMMP_USE_DISPLAY_POINTS
    );

    if (count > 0)
    {
        for (int i = 0; i < count; ++i)
        {
            outPoints.push_back({ points[i].x, points[i].y });
        }
    }
}
```

第二种是使用曲线插值——在两个相邻采样点之间插入贝塞尔曲线或 Catmull-Rom 样条来平滑轨迹。这种方法不依赖更高的采样率，而是在后处理阶段让线条变得更平滑：

```cpp
// 简单的线性插值——在两个采样点之间插入中间点
void InterpolateLine(POINT p1, POINT p2, float stepPixels,
                     std::vector<POINT>& outPoints)
{
    float dx = static_cast<float>(p2.x - p1.x);
    float dy = static_cast<float>(p2.y - p1.y);
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < stepPixels)
    {
        outPoints.push_back(p2);
        return;
    }

    int steps = static_cast<int>(dist / stepPixels);
    for (int i = 1; i <= steps; ++i)
    {
        float t = static_cast<float>(i) / steps;
        POINT interp;
        interp.x = static_cast<LONG>(p1.x + dx * t);
        interp.y = static_cast<LONG>(p1.y + dy * t);
        outPoints.push_back(interp);
    }
}
```

对于大多数应用场景来说，第二种方法（曲线插值）已经足够了。`GetMouseMovePointsEx` 主要用于需要极高精度追踪的应用，比如数字签名或手写识别。

## ⚠️ 踩坑预警

**坑点一：忘记 ReleaseCapture 的严重后果**

这一点怎么强调都不过分。如果你在 `SetCapture` 后没有在合适的时机调用 `ReleaseCapture`，用户会发现整个系统的鼠标交互都被你的窗口"霸占"了——点击桌面没反应、点击任务栏没反应、点击其他窗口没反应。唯一的恢复方式是用键盘（Alt+F4 或 Ctrl+Alt+Delete）来关闭你的程序。所以在任何可能退出拖拽逻辑的路径上，都要确保 `ReleaseCapture` 被调用了。

**坑点二：WM_MOUSEMOVE 坐标可能超出窗口范围**

在鼠标捕获状态下，`WM_MOUSEMOVE` 的 `lParam` 中的坐标可能远超出窗口客户区域——可能是负数，也可能大于窗口尺寸。如果你的绘制代码没有做边界检查，可能会导致越界访问或者绘制到屏幕的其他位置。在处理捕获状态下的鼠标移动时，始终要做边界裁剪。

**坑点三：选框绘制的闪烁问题**

如果你不用双缓冲来绘制 Rubber Band 选框，而直接在屏幕 DC 上用 XOR 模式绘制，那么在高速拖拽时可能会看到选框闪烁。这是因为"擦除旧选框→绘制新选框"之间的时间间隔被人眼感知到了。解决方法是使用双缓冲（在内存 DC 上完成所有绘制后一次性拷贝），或者使用 Direct2D 的硬件加速渲染。

**坑点四：PtInRect 的边界判断**

`PtInRect` 对矩形右边和下边是"不包含"的（开区间）。如果你需要精确到像素级别的命中测试，这个行为可能会导致一些微妙的 bug——比如一个 100x100 的矩形，点 (100, 50) 会被判定为不在矩形内。如果你的需求是"包含边界"，需要手动调整判断逻辑。

## 常见问题

**Q: FillContainsPoint 和 StrokeContainsPoint 的性能如何？**

A: 对于简单的几何体（圆形、矩形、椭圆），`FillContainsPoint` 的性能非常好——基本上就是几个数学公式的事情。对于复杂的路径几何体（包含数百个段的贝塞尔路径），性能会下降，但通常仍在可接受范围内。如果你需要在一个包含数千个形状的画布上做实时命中测试，建议先用包围盒（`GetBounds`）做粗筛，只有包围盒命中的形状才做精确几何测试。

**Q: 什么时候用 SetCapture，什么时候不用？**

A: 简单的原则：如果你的交互涉及"按住→移动→松开"的三段式操作，就需要 `SetCapture`。比如拖拽、Rubber Band 选择、滑块拖动等。如果只是简单的点击（按下即生效，不需要松开确认），则不需要捕获鼠标。

**Q: 如何实现"点击穿透但有条件的"——比如点击半透明叠加层的空白区域时穿透，但点击按钮时不穿透？**

A: 这需要叠加层自己处理 `WM_NCHITTEST`——在命中测试函数中判断鼠标是否在"可交互"区域上。如果在按钮上，返回 `HTCLIENT`（不穿透）；如果在空白区域上，返回 `HTTRANSPARENT`（穿透）。对于非矩形的可交互区域，需要使用精确的几何命中测试。

**Q: 多显示器环境下鼠标捕获的行为？**

A: `SetCapture` 在多显示器环境下正常工作——即使鼠标移到了另一个显示器上，你的窗口仍然能收到鼠标消息。不过坐标可能非常大或非常小（负数），需要确保你的代码能正确处理这些极端值。

## 总结

命中测试和鼠标事件路由是 GUI 交互的底层基础设施。今天我们从 `PtInRect` 的矩形判断出发，了解了 Direct2D Geometry 的精确几何命中测试（`FillContainsPoint` 和 `StrokeContainsPoint`），然后深入了 `SetCapture`/`ReleaseCapture` 的鼠标捕获机制，并用这些知识实现了一个完整的 Rubber Band 拖拽选择框。

我们还讨论了 `WS_EX_TRANSPARENT` 的真正含义（绘制顺序而非事件穿透）、Z 序对命中测试优先级的影响、以及鼠标采样不足的插值解决方案。这些知识点在构建复杂交互应用时都非常有用。

接下来，我们的视线将从 2D 图形转向 GPU 加速的 3D 渲染。下一篇我们将讨论如何在 Win32 窗口中嵌入 OpenGL——从 WGL（Windows GL）胶合层到现代 Core Profile 上下文的创建，从 `PIXELFORMATDESCRIPTOR` 到 `wglCreateContextAttribsARB` 的两步法。这是进入 GPU 渲染世界的大门。

---

## 练习

1. **基础练习——精确圆形命中测试**：创建一个包含多个圆形的画布，使用 `ID2D1EllipseGeometry` 和 `FillContainsPoint` 实现精确的圆形命中测试。验证矩形包围盒命中的区域在精确测试中被正确排除。

2. **进阶练习——矩形选框工具**：在绘图板上实现一个矩形选框工具。要求：按住鼠标左键拖拽显示虚线选框（Rubber Band），选框实时显示尺寸标注（宽x高像素），松开后选区保留在画面上（高亮显示被选中的区域）。

3. **实战练习——多选拖拽**：在练习 2 的基础上，实现选中多个形状后拖拽移动的功能。提示：需要维护一个"选中列表"和"拖拽偏移量"，在 `WM_MOUSEMOVE` 中更新所有选中形状的位置。注意处理拖出窗口边界的情况。

4. **思考题——不规则选区**：如果选框不是矩形而是自由绘制的套索形状（Lasso Selection），你会如何实现命中测试？提示：将套索路径转换为 `ID2D1PathGeometry`，然后用 `FillContainsPoint` 做命中测试。考虑路径闭合和自相交的处理。

---

**参考资料**:
- [PtInRect function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-ptinrect)
- [ID2D1Geometry::FillContainsPoint - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1geometry-fillcontainspoint(d2d1_point_2f_constd2d1_matrix_3x2_f_float_bool))
- [ID2D1Geometry interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1geometry)
- [SetCapture function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)
- [ReleaseCapture function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-releasecapture)
- [WM_NCHITTEST message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [WS_EX_TRANSPARENT - Raymond Chen's blog](https://devblogs.microsoft.com/oldnewthing/20121217-00/?p=5823)
- [Extended Window Styles - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- [GetMouseMovePointsEx function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmousemovepointsex)
