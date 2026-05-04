/**
 * 06_double_buffer - GDI 双缓冲示例
 *
 * 演示双缓冲（Double Buffering）消除绘图闪烁的原理：
 * 1. 无双缓冲（左半部分）: 在 WM_PAINT 中直接绘制大量矩形，产生可见闪烁
 * 2. 有双缓冲（右半部分）: 先绘制到内存 DC，再 BitBlt 一次性复制到屏幕
 * 3. WM_ERASEBKGND 处理返回 1，阻止系统擦除背景，减少闪烁源
 * 4. WM_TIMER (50ms) 持续触发重绘，使闪烁差异清晰可见
 * 5. 复选框切换"直接绘制"和"双缓冲"模式
 * 6. 标题栏显示帧计数
 *
 * 参考: tutorial/native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cstdio>

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"GdiDoubleBufferClass";

// 窗口尺寸
static const int WINDOW_WIDTH  = 600;
static const int WINDOW_HEIGHT = 400;

// 定时器 ID 与间隔
static const int TIMER_ID       = 1;
static const int TIMER_INTERVAL = 50;  // 毫秒

// 复选框控件 ID
static const int IDC_CHECKBOX   = 101;

// 全局状态
static bool   g_useDoubleBuffer = true;   // 是否使用双缓冲
static int    g_frameCount      = 0;      // 帧计数
static HWND   g_hCheckbox       = nullptr;
static int    g_animOffset      = 0;      // 动画偏移量

/**
 * 绘制大量彩色矩形（模拟复杂绘图操作）
 *
 * 这些矩形会产生明显的视觉闪烁（直接绘制模式下）。
 * 动画偏移量使矩形不断变化，确保每帧都有重绘。
 *
 * 参数:
 *   hdc      - 目标设备上下文
 *   x, y     - 绘制区域左上角
 *   width    - 绘制区域宽度
 *   height   - 绘制区域高度
 */
static void DrawColoredRects(HDC hdc, int x, int y, int width, int height)
{
    // 定义颜色数组
    COLORREF colors[] = {
        RGB(255,  80,  80),   // 红
        RGB( 80, 200,  80),   // 绿
        RGB( 80,  80, 255),   // 蓝
        RGB(255, 255,  80),   // 黄
        RGB(255,  80, 255),   // 品红
        RGB( 80, 255, 255),   // 青
        RGB(255, 160,  80),   // 橙
        RGB(160,  80, 255),   // 紫
    };
    int numColors = sizeof(colors) / sizeof(colors[0]);

    // 绘制多行多列的矩形阵列
    int cols = 5;
    int rows = 4;
    int cellW = width  / cols;
    int cellH = height / rows;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            // 根据动画偏移量选择颜色，使颜色不断变化
            int colorIdx = (r * cols + c + g_animOffset) % numColors;

            // 添加一些随机偏移使每帧看起来不同
            int shrinkX = (g_animOffset + r + c) % 8;
            int shrinkY = (g_animOffset + r * 3 + c * 7) % 6;

            HBRUSH hBrush = CreateSolidBrush(colors[colorIdx]);
            RECT rc = {
                x + c * cellW + shrinkX,
                y + r * cellH + shrinkY,
                x + (c + 1) * cellW - shrinkX,
                y + (r + 1) * cellH - shrinkY
            };
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
        }
    }
}

/**
 * 绘制面板标题与说明
 */
static void DrawPanelLabel(HDC hdc, const wchar_t* title, const wchar_t* desc,
                           int x, int y, int width)
{
    SetBkMode(hdc, TRANSPARENT);

    // 标题
    HFONT hTitleFont = CreateFont(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);
    SetTextColor(hdc, RGB(30, 30, 30));
    RECT rcTitle = { x, y, x + width, y + 24 };
    DrawText(hdc, title, -1, &rcTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 说明
    SelectObject(hdc, hOldFont);
    DeleteObject(hTitleFont);

    HFONT hDescFont = CreateFont(
        13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hDescFont);
    SetTextColor(hdc, RGB(100, 100, 100));
    RECT rcDesc = { x, y + 24, x + width, y + 44 };
    DrawText(hdc, desc, -1, &rcDesc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hDescFont);
}

/**
 * 更新标题栏，显示帧计数
 */
static void UpdateTitleBar(HWND hwnd)
{
    wchar_t szTitle[128];
    _snwprintf(szTitle, 128, L"GDI 双缓冲示例 - 帧: %d [%s]",
               g_frameCount,
               g_useDoubleBuffer ? L"双缓冲" : L"直接绘制");
    SetWindowText(hwnd, szTitle);
}

/**
 * 窗口过程函数
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建复选框控件 —— 切换双缓冲开关
        g_hCheckbox = CreateWindowEx(
            0, L"BUTTON", L"使用双缓冲",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, 8, 160, 24,
            hwnd, (HMENU)IDC_CHECKBOX,
            ((LPCREATESTRUCT)lParam)->hInstance, nullptr
        );
        // 设置初始选中状态
        SendMessage(g_hCheckbox, BM_SETCHECK, BST_CHECKED, 0);

        // 启动定时器
        SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);
        return 0;
    }

    case WM_COMMAND:
    {
        // 处理复选框状态变化
        if (LOWORD(wParam) == IDC_CHECKBOX && HIWORD(wParam) == BN_CLICKED)
        {
            // 获取复选框状态
            LRESULT state = SendMessage(g_hCheckbox, BM_GETCHECK, 0, 0);
            g_useDoubleBuffer = (state == BST_CHECKED);
        }
        return 0;
    }

    /**
     * WM_ERASEBKGND - 擦除背景消息
     *
     * 返回 1 表示"我已经处理了背景擦除"，
     * 实际上我们什么都不做，这样系统就不会用背景色填充窗口，
     * 从而消除一个重要的闪烁源。
     */
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
    {
        // 定时器触发 —— 更新动画偏移并触发重绘
        g_animOffset = (g_animOffset + 1) % 100;
        InvalidateRect(hwnd, nullptr, FALSE);  // FALSE = 不擦除背景
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int clientW = rcClient.right;
        int clientH = rcClient.bottom;

        int topOffset = 40;   // 顶部控件区域
        int midX = clientW / 2;

        if (g_useDoubleBuffer)
        {
            // ================================================================
            // 双缓冲模式：两个面板都使用双缓冲绘制
            // ================================================================

            // --- 创建全窗口大小的内存 DC ---
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

            // 先在内存 DC 中填充白色背景
            HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
            RECT rcFull = { 0, 0, clientW, clientH };
            FillRect(hdcMem, &rcFull, hWhite);

            // 左侧面板 —— "无双缓冲效果演示"
            DrawPanelLabel(hdcMem,
                           L"双缓冲绘制 (左)", L"无闪烁",
                           0, topOffset, midX);
            DrawColoredRects(hdcMem, 4, topOffset + 48, midX - 8, clientH - topOffset - 56);

            // 右侧面板 —— "有双缓冲效果演示"
            DrawPanelLabel(hdcMem,
                           L"双缓冲绘制 (右)", L"无闪烁",
                           midX, topOffset, midX);
            DrawColoredRects(hdcMem, midX + 4, topOffset + 48, midX - 8, clientH - topOffset - 56);

            // 一次性将内存 DC 复制到屏幕 —— 只有一次 BitBlt，无闪烁
            BitBlt(hdc, 0, 0, clientW, clientH, hdcMem, 0, 0, SRCCOPY);

            // 清理
            SelectObject(hdcMem, hOldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdcMem);
        }
        else
        {
            // ================================================================
            // 直接绘制模式：直接在屏幕 DC 上绘制，会产生闪烁
            // ================================================================

            // 先填充白色背景
            HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
            RECT rcFull = { 0, 0, clientW, clientH };
            FillRect(hdc, &rcFull, hWhite);

            // 左侧面板 —— 直接绘制
            DrawPanelLabel(hdc,
                           L"直接绘制 (左)", L"注意闪烁!",
                           0, topOffset, midX);
            // 每次调用 FillRect 都是一次独立的屏幕绘制操作，产生闪烁
            DrawColoredRects(hdc, 4, topOffset + 48, midX - 8, clientH - topOffset - 56);

            // 右侧面板 —— 直接绘制
            DrawPanelLabel(hdc,
                           L"直接绘制 (右)", L"注意闪烁!",
                           midX, topOffset, midX);
            DrawColoredRects(hdc, midX + 4, topOffset + 48, midX - 8, clientH - topOffset - 56);
        }

        // 绘制分隔线
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, midX, topOffset, nullptr);
        LineTo(hdc, midX, clientH);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);

        // 更新帧计数与标题栏
        g_frameCount++;
        UpdateTitleBar(hwnd);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/**
 * 注册窗口类
 */
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // 双缓冲模式下不需要系统背景
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

/**
 * 创建主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"GDI 双缓冲示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

/**
 * wWinMain - 应用程序入口点
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    if (!RegisterWindowClass(hInstance))
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
