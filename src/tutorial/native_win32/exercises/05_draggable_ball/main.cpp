/**
 * Win32 可拖动小球练习程序
 *
 * 本程序演示：
 * 1. 鼠标左键按下检测 (WM_LBUTTONDOWN)
 * 2. 鼠标移动跟踪 (WM_MOUSEMOVE)
 * 3. 鼠标左键释放 (WM_LBUTTONUP)
 * 4. 点到圆的距离计算
 * 5. 光标动态切换 (SetCursor)
 * 6. 触发重绘 (InvalidateRect)
 * 7. GDI 椭圆绘制 (Ellipse)
 *
 * 编译方式:
 *   cmake -B build
 *   cmake --build build
 */

#include <windows.h>
#include <cmath>
#include <wchar.h>  // swprintf

// =============================================================================
// 全局变量
// =============================================================================

// 窗口类名
const PCWSTR WINDOW_CLASS_NAME = L"DraggableBallWindow";

// 小球状态
struct Ball
{
    int centerX;    // 圆心 X 坐标
    int centerY;    // 圆心 Y 坐标
    int radius;     // 半径
    COLORREF color; // 颜色

    Ball()
        : centerX(300)
        , centerY(200)
        , radius(50)
        , color(RGB(255, 100, 100))
    {}
};

Ball g_ball;

// 拖动状态
struct DragState
{
    bool isDragging;      // 是否正在拖动
    int offsetX;          // 鼠标点击位置相对于圆心的偏移 X
    int offsetY;          // 鼠标点击位置相对于圆心的偏移 Y

    DragState()
        : isDragging(false)
        , offsetX(0)
        , offsetY(0)
    {}
};

DragState g_dragState;

// 手型光标（预先加载）
HCURSOR g_hHandCursor = nullptr;

// =============================================================================
// 辅助函数
// =============================================================================

/**
 * 计算两点之间的距离平方
 *
 * 使用距离平方而不是实际距离可以避免昂贵的 sqrt 运算
 * 当只需要比较距离时（如判断是否在圆内），使用平方即可
 */
int DistanceSquared(int x1, int y1, int x2, int y2)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    return dx * dx + dy * dy;
}

/**
 * 检测点是否在圆内
 */
bool IsPointInCircle(int x, int y, const Ball& ball)
{
    // 点到圆心的距离平方 <= 半径平方
    return DistanceSquared(x, y, ball.centerX, ball.centerY)
           <= ball.radius * ball.radius;
}

/**
 * 绘制填充圆形
 */
void DrawCircle(HDC hdc, int centerX, int centerY, int radius, COLORREF color)
{
    // 创建画刷
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // 创建画笔（黑色边框）
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    // Ellipse 使用边界矩形定义椭圆
    Ellipse(hdc,
            centerX - radius, centerY - radius,
            centerX + radius, centerY + radius);

    // 恢复并删除 GDI 对象
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

/**
 * 绘制提示文本
 */
void DrawInstructions(HDC hdc)
{
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    TextOut(hdc, 20, 20, L"可拖动小球练习", 7);
    TextOut(hdc, 20, 45, L"- 用鼠标左键拖动红色小球", 12);
    TextOut(hdc, 20, 65, L"- 拖动时光标变为手型", 11);

    // 显示当前状态
    wchar_t statusText[100];
    if (g_dragState.isDragging)
    {
        swprintf(statusText, 100, L"状态: 拖动中 (位置: %d, %d)",
                 g_ball.centerX, g_ball.centerY);
    }
    else
    {
        swprintf(statusText, 100, L"状态: 等待 (位置: %d, %d)",
                 g_ball.centerX, g_ball.centerY);
    }
    TextOut(hdc, 20, 90, statusText, static_cast<int>(wcslen(statusText)));
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
        // 窗口创建时加载手型光标
        g_hHandCursor = LoadCursor(nullptr, IDC_HAND);
        return 0;

    case WM_LBUTTONDOWN:
    {
        // 获取鼠标位置
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // 检测是否点击在小球内
        if (IsPointInCircle(x, y, g_ball))
        {
            // 开始拖动
            g_dragState.isDragging = true;

            // 记录鼠标点击位置相对于圆心的偏移
            // 这样拖动时小球不会"跳"到鼠标位置
            g_dragState.offsetX = x - g_ball.centerX;
            g_dragState.offsetY = y - g_ball.centerY;

            // 设置捕获，确保即使鼠标移出窗口也能接收到鼠标消息
            SetCapture(hwnd);

            // 改变光标为手型
            SetCursor(g_hHandCursor);

            // 触发重绘以更新显示
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        // 获取鼠标位置
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        if (g_dragState.isDragging)
        {
            // 正在拖动 - 更新小球位置
            g_ball.centerX = x - g_dragState.offsetX;
            g_ball.centerY = y - g_dragState.offsetY;

            // 确保光标保持为手型
            SetCursor(g_hHandCursor);

            // 触发重绘
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else
        {
            // 未拖动 - 检查鼠标是否在小球上，改变光标提示可拖动
            if (IsPointInCircle(x, y, g_ball))
            {
                SetCursor(g_hHandCursor);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_dragState.isDragging)
        {
            // 结束拖动
            g_dragState.isDragging = false;

            // 释放鼠标捕获
            ReleaseCapture();

            // 触发重绘以更新状态显示
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
    {
        // 当鼠标捕获被改变（如失去焦点）时，结束拖动状态
        // 这是一个安全措施，防止拖动状态卡住
        if (g_dragState.isDragging)
        {
            g_dragState.isDragging = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int cx = rcClient.right - rcClient.left;
        int cy = rcClient.bottom - rcClient.top;

        // 绘制白色背景
        HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcClient, hBgBrush);
        DeleteObject(hBgBrush);

        // 绘制提示文本
        DrawInstructions(hdc);

        // 绘制小球
        DrawCircle(hdc, g_ball.centerX, g_ball.centerY,
                   g_ball.radius, g_ball.color);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        // 在 WM_PAINT 中处理背景，避免闪烁
        return 1;

    case WM_DESTROY:
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
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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
        0,
        WINDOW_CLASS_NAME,
        L"可拖动小球练习",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        nullptr,
        nullptr,
        hInstance,
        nullptr
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
