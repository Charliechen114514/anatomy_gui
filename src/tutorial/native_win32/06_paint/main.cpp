/**
 * Win32 Paint 示例程序
 *
 * 本程序演示 Win32 GDI 绘图的基本概念和技术：
 * 1. WM_PAINT 消息处理 - BeginPaint/EndPaint
 * 2. 多种 GDI 绘图函数 - TextOut, Rectangle, Ellipse, Line
 * 3. GDI 对象的创建和销毁 - CreateSolidBrush, DeleteObject
 * 4. 更新区域 (Update Region) 的概念
 * 5. 使用定时器触发定期重绘
 *
 * 编译方式:
 *   cmake -B build
 *   cmake --build build
 *
 * 参考: tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md
 *       "绘制窗口 -> WM_PAINT 消息"
 */

#include <windows.h>
#include <string>
#include <cmath>

// =============================================================================
// 全局变量 - 用于演示状态管理
// =============================================================================

// 窗口类名
const PCWSTR WINDOW_CLASS_NAME = L"PaintExampleWindow";

// 定时器 ID
const UINT_PTR TIMER_ID = 1;

// 动画状态 - 用于演示定时器触发的重绘
struct AnimationState
{
    float angle;           // 旋转角度
    int rectX;             // 矩形 X 位置
    int rectY;             // 矩形 Y 位置
    int rectDX;            // 矩形 X 方向速度
    int rectDY;            // 矩形 Y 方向速度

    AnimationState()
        : angle(0.0f)
        , rectX(200)
        , rectY(200)
        , rectDX(3)
        , rectDY(2)
    {}
};

AnimationState g_animation;

// =============================================================================
// 辅助函数 - GDI 绘图演示
// =============================================================================

/**
 * 绘制文本 - 演示 TextOut 函数
 */
void DrawText(HDC hdc, int x, int y, const wchar_t* text)
{
    TextOut(hdc, x, y, text, static_cast<int>(wcslen(text)));
}

/**
 * 绘制填充矩形 - 演示 Rectangle 和画刷的使用
 */
void DrawFilledRect(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    // 创建纯色画刷
    HBRUSH hBrush = CreateSolidBrush(color);

    // 保存原来的画刷
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // 绘制矩形（Rectangle 会使用当前选中的画刷填充）
    Rectangle(hdc, left, top, right, bottom);

    // 恢复原来的画刷
    SelectObject(hdc, hOldBrush);

    // 删除我们创建的画刷（重要：释放 GDI 对象！）
    DeleteObject(hBrush);
}

/**
 * 绘制空心矩形 - 使用库存画刷
 */
void DrawHollowRect(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    // 使用库存的空画刷（HOLLOW_BRUSH）
    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    // 创建并选择画笔
    HPEN hPen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    // 绘制矩形（不会填充，只绘制边框）
    Rectangle(hdc, left, top, right, bottom);

    // 恢复并删除
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

/**
 * 绘制椭圆 - 演示 Ellipse 函数
 */
void DrawEllipse(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // Ellipse 使用边界矩形定义椭圆
    Ellipse(hdc, left, top, right, bottom);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
}

/**
 * 绘制圆形
 */
void DrawCircle(HDC hdc, int centerX, int centerY, int radius, COLORREF color)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // 圆形是特殊的椭圆
    Ellipse(hdc, centerX - radius, centerY - radius,
            centerX + radius, centerY + radius);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
}

/**
 * 绘制线条 - 演示 LineTo 和 MoveToEx
 */
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1)
{
    // 创建画笔
    HPEN hPen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    // 移动到起点
    MoveToEx(hdc, x1, y1, nullptr);

    // 绘制到终点
    LineTo(hdc, x2, y2);

    // 恢复并删除
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

/**
 * 绘制多边形 - 演示 Polygon 函数
 */
void DrawTriangle(HDC hdc, POINT p1, POINT p2, POINT p3, COLORREF color)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    POINT points[] = { p1, p2, p3 };
    Polygon(hdc, points, 3);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
}

/**
 * 绘制旋转的矩形 - 演示更复杂的绘图
 */
void DrawRotatingRect(HDC hdc, int centerX, int centerY, int size, float angle, COLORREF color)
{
    // 计算旋转后的四个顶点
    float cosA = cos(angle);
    float sinA = sin(angle);
    float halfSize = size / 2.0f;

    POINT points[4];
    points[0].x = static_cast<LONG>(centerX + (-halfSize * cosA - (-halfSize) * sinA));
    points[0].y = static_cast<LONG>(centerY + (-halfSize * sinA + (-halfSize) * cosA));

    points[1].x = static_cast<LONG>(centerX + (halfSize * cosA - (-halfSize) * sinA));
    points[1].y = static_cast<LONG>(centerY + (halfSize * sinA + (-halfSize) * cosA));

    points[2].x = static_cast<LONG>(centerX + (halfSize * cosA - halfSize * sinA));
    points[2].y = static_cast<LONG>(centerY + (halfSize * sinA + halfSize * cosA));

    points[3].x = static_cast<LONG>(centerX + (-halfSize * cosA - halfSize * sinA));
    points[3].y = static_cast<LONG>(centerY + (-halfSize * sinA + halfSize * cosA));

    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    Polygon(hdc, points, 4);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
}

/**
 * 绘制静态场景 - 不需要动画的部分
 */
void DrawStaticScene(HDC hdc, int cx, int cy)
{
    // 设置文本颜色
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);  // 透明背景

    // 标题
    DrawText(hdc, 20, 20, L"Win32 GDI 绘图示例");
    DrawText(hdc, 20, 45, L"演示 WM_PAINT 消息处理和 GDI 绘图函数");

    // 绘制不同颜色的矩形
    DrawFilledRect(hdc, 20, 80, 120, 180, RGB(255, 0, 0));      // 红色矩形
    DrawFilledRect(hdc, 130, 80, 230, 180, RGB(0, 255, 0));     // 绿色矩形
    DrawFilledRect(hdc, 240, 80, 340, 180, RGB(0, 0, 255));     // 蓝色矩形

    // 绘制椭圆
    DrawEllipse(hdc, 20, 200, 120, 300, RGB(255, 255, 0));      // 黄色椭圆
    DrawEllipse(hdc, 130, 200, 230, 300, RGB(255, 0, 255));     // 品红椭圆

    // 绘制圆形
    DrawCircle(hdc, 285, 250, 50, RGB(0, 255, 255));            // 青色圆形

    // 绘制空心矩形
    DrawHollowRect(hdc, 20, 320, 150, 400, RGB(128, 128, 128));

    // 绘制三角形
    POINT p1 = { 200, 320 }, p2 = { 260, 400 }, p3 = { 140, 400 };
    DrawTriangle(hdc, p1, p2, p3, RGB(255, 128, 0));

    // 绘制线条
    DrawLine(hdc, 20, 420, 340, 420, RGB(0, 0, 0), 1);          // 细线
    DrawLine(hdc, 20, 440, 340, 440, RGB(0, 0, 0), 3);          // 粗线

    // 说明文字
    RECT rc = { 370, 80, 600, 500 };
    DrawText(hdc, 370, 80, L"功能说明:");
    DrawText(hdc, 370, 105, L"- Rectangle: 填充矩形");
    DrawText(hdc, 370, 130, L"- Ellipse: 椭圆/圆形");
    DrawText(hdc, 370, 155, L"- TextOut: 文本输出");
    DrawText(hdc, 370, 180, L"- LineTo/MoveToEx: 线条");
    DrawText(hdc, 370, 205, L"- Polygon: 多边形");
    DrawText(hdc, 370, 230, L"- CreateSolidBrush: 画刷");
    DrawText(hdc, 370, 255, L"- CreatePen: 画笔");
    DrawText(hdc, 370, 280, L"- 定时器触发重绘");
    DrawText(hdc, 370, 305, L"- 动态矩形碰撞反弹");
    DrawText(hdc, 370, 330, L"- 旋转矩形动画");

    // 分隔线
    DrawLine(hdc, 360, 70, 360, 500, RGB(200, 200, 200), 2);
}

/**
 * 绘制动画场景 - 需要定时更新的部分
 */
void DrawAnimatedScene(HDC hdc, int cx, int cy)
{
    // 动画区域说明
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, 20, 470, L"动画区域 (定时器驱动):");

    // 绘制边界框
    DrawHollowRect(hdc, 20, 490, 340, 600, RGB(0, 0, 0));

    // 更新并绘制反弹矩形
    g_animation.rectX += g_animation.rectDX;
    g_animation.rectY += g_animation.rectDY;

    // 边界检测
    if (g_animation.rectX <= 20 || g_animation.rectX + 60 >= 340)
    {
        g_animation.rectDX = -g_animation.rectDX;
    }
    if (g_animation.rectY <= 490 || g_animation.rectY + 40 >= 600)
    {
        g_animation.rectDY = -g_animation.rectDY;
    }

    // 绘制反弹矩形
    DrawFilledRect(hdc, g_animation.rectX, g_animation.rectY,
                   g_animation.rectX + 60, g_animation.rectY + 40,
                   RGB(255, 100, 50));

    // 绘制旋转矩形
    g_animation.angle += 0.05f;  // 每次增加角度
    DrawRotatingRect(hdc, 500, 545, 80, g_animation.angle, RGB(100, 150, 255));

    // 绘制旋转矩形的中心点
    DrawCircle(hdc, 500, 545, 5, RGB(0, 0, 0));
}

// =============================================================================
// 窗口过程函数
// =============================================================================

/**
 * 窗口过程 - 处理窗口消息
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        // 窗口创建时启动定时器
        // 定时器每 16ms 触发一次（约 60 FPS）
        SetTimer(hwnd, TIMER_ID, 16, nullptr);
        return 0;

    case WM_TIMER:
        // 定时器触发 - 请求重绘
        // InvalidateRect 标记整个客户区需要重绘
        // 这会导致系统发送 WM_PAINT 消息
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
    {
        // ===============================================
        // WM_PAINT 处理 - 核心绘图逻辑
        // ===============================================

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // ps.rcPaint 是需要重绘的区域（更新区域）
        // 只有这个区域内的绘图才是必需的，其他区域会被系统裁剪
        RECT& rcPaint = ps.rcPaint;

        // 获取客户区大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int cx = rcClient.right - rcClient.left;
        int cy = rcClient.bottom - rcClient.top;

        // 可以在这里检查更新区域，优化绘图性能
        // 例如：如果更新区域不包含动画区域，可以跳过动画绘制

        // 绘制白色背景
        // 注意：在实际应用中，背景通常由窗口类的 hbrBackground 处理
        // 这里我们手动绘制以便演示
        DrawFilledRect(hdc, 0, 0, cx, cy, RGB(255, 255, 255));

        // 绘制静态场景
        DrawStaticScene(hdc, cx, cy);

        // 绘制动画场景
        DrawAnimatedScene(hdc, cx, cy);

        // ===============================================
        // EndPaint - 结束绘图
        // ===============================================
        // EndPaint 会：
        // 1. 释放设备上下文 (DC)
        // 2. 验证更新区域（标记为已处理）
        // 3. 释放 BeginPaint 获取的 DC
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        // 返回 TRUE 表示我们已经处理了背景
        // 在 WM_PAINT 中我们绘制了白色背景，所以这里跳过默认的背景擦除
        // 这样可以避免闪烁
        return 1;

    case WM_DESTROY:
        // 窗口销毁时停止定时器
        KillTimer(hwnd, TIMER_ID);

        // 发送退出消息
        PostQuitMessage(0);
        return 0;
    }

    // 默认消息处理
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// =============================================================================
// 主函数
// =============================================================================

/**
 * 注册窗口类
 */
void RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = { };

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;  // 窗口大小改变时重绘
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  // 默认背景
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassEx(&wc);
}

/**
 * WinMain - 程序入口点
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    // 注册窗口类
    RegisterWindowClass(hInstance);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展样式
        WINDOW_CLASS_NAME,              // 窗口类名
        L"Win32 Paint 示例 - GDI 绘图",  // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,   // 位置
        650, 700,                       // 大小
        nullptr,                        // 父窗口
        nullptr,                        // 菜单
        hInstance,                      // 实例句柄
        nullptr                         // 附加数据
    );

    if (hwnd == nullptr)
    {
        return 0;
    }

    // 显示和更新窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
