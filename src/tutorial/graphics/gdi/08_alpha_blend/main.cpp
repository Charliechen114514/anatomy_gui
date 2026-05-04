/**
 * 08_alpha_blend - GDI Alpha 混合示例
 *
 * 演示 GDI 的 Alpha 混合 (Alpha Blending) 功能：
 * 1. AlphaBlend (msimg32.lib) — 绘制半透明彩色矩形覆盖在背景上
 * 2. 常量 Alpha: BLENDFUNCTION 中 SourceConstantAlpha 从 0（完全透明）到 255（不透明）
 * 3. 逐像素 Alpha: 创建 32 位 DIB Section，每个像素带有独立的 Alpha 通道
 * 4. TrackBar 控件 (TRACKBAR_CLASS) 动态调节 Alpha 值 (0-255)
 * 5. 背景: 棋盘格图案用于直观展示透明效果
 * 6. 三个重叠的半透明矩形 (红/绿/蓝) 混合在一起
 *
 * 依赖库: msimg32.lib (AlphaBlend), comctl32.lib (TrackBar)
 *
 * 参考: tutorial/native_win32/25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <commctrl.h>

// 需要链接的库
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comctl32.lib")

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"GdiAlphaBlendClass";

// 窗口尺寸
static const int WINDOW_WIDTH  = 600;
static const int WINDOW_HEIGHT = 500;

// TrackBar 控件 ID
static const int IDC_TRACKBAR = 101;

// 全局 Alpha 值（由 TrackBar 控制）
static int g_alphaValue = 128;  // 默认半透明

/**
 * 绘制棋盘格背景
 *
 * 棋盘格是展示透明效果的标准背景图案：
 * 透明区域可以看到灰色/白色交替的方块。
 *
 * 参数:
 *   hdc    - 目标设备上下文
 *   x, y   - 左上角坐标
 *   width  - 宽度
 *   height - 高度
 *   cellSize - 每个方块的尺寸（像素）
 */
static void DrawCheckerboard(HDC hdc, int x, int y, int width, int height, int cellSize)
{
    HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
    HBRUSH hGray  = CreateSolidBrush(RGB(210, 210, 210));

    for (int row = 0; row * cellSize < height; row++)
    {
        for (int col = 0; col * cellSize < width; col++)
        {
            HBRUSH hBrush = ((row + col) % 2 == 0) ? hWhite : hGray;
            RECT rc = {
                x + col * cellSize,
                y + row * cellSize,
                std::min(x + (col + 1) * cellSize, x + width),
                std::min(y + (row + 1) * cellSize, y + height)
            };
            FillRect(hdc, &rc, hBrush);
        }
    }

    DeleteObject(hGray);
}

/**
 * 绘制三个重叠的半透明矩形 (红/绿/蓝)
 *
 * 使用 AlphaBlend 将半透明彩色矩形绘制到指定区域。
 * 三个矩形部分重叠，可以观察到 Alpha 混合的叠加效果。
 *
 * 参数:
 *   hdc   - 目标设备上下文（应已有棋盘格背景）
 *   x, y  - 绘制区域左上角
 *   size  - 绘制区域尺寸（正方形）
 *   alpha - Alpha 值 (0=全透明, 255=不透明)
 */
static void DrawOverlappingRects(HDC hdc, int x, int y, int size, BYTE alpha)
{
    int rectW = size * 2 / 3;
    int rectH = size * 2 / 3;

    // 定义三个矩形的位置和颜色
    struct { int dx, dy; COLORREF color; } rects[] = {
        { 0,             0,             RGB(255, 40, 40) },   // 红 - 左上
        { size / 3,      0,             RGB(40, 255, 40) },   // 绿 - 中上
        { size / 6,      size / 3,      RGB(40, 40, 255) },   // 蓝 - 中下
    };

    for (int i = 0; i < 3; i++)
    {
        // 为每个颜色创建一个纯色位图
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rectW, rectH);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

        // 用纯色填充整个位图
        HBRUSH hBrush = CreateSolidBrush(rects[i].color);
        RECT rcFill = { 0, 0, rectW, rectH };
        FillRect(hdcMem, &rcFill, hBrush);
        DeleteObject(hBrush);

        // --- AlphaBlend: 半透明混合 ---
        // BLENDFUNCTION 结构控制混合方式
        BLENDFUNCTION bf;
        bf.BlendOp             = AC_SRC_OVER;        // 混合操作：源覆盖目标
        bf.BlendFlags          = 0;                   // 必须为 0
        bf.SourceConstantAlpha = alpha;               // 常量 Alpha 值 (0-255)
        bf.AlphaFormat         = 0;                   // 0 = 使用常量 Alpha

        AlphaBlend(
            hdc,                                // 目标 DC
            x + rects[i].dx,                    // 目标 X
            y + rects[i].dy,                    // 目标 Y
            rectW,                              // 目标宽度
            rectH,                              // 目标高度
            hdcMem,                             // 源 DC
            0, 0,                               // 源位置
            rectW,                              // 源宽度
            rectH,                              // 源高度
            bf                                  // 混合函数
        );

        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
    }
}

/**
 * 绘制逐像素 Alpha 演示
 *
 * 创建一个 32 位 DIB Section，每个像素有独立的 Alpha 通道。
 * 展示从左到右 Alpha 从 0 渐变到 255 的效果。
 *
 * 参数:
 *   hdc    - 目标设备上下文
 *   x, y   - 绘制位置
 *   width  - 宽度
 *   height - 高度
 */
static void DrawPerPixelAlpha(HDC hdc, int x, int y, int width, int height)
{
    // 创建 32 位 DIB（带 Alpha 通道）
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = height;    // 正值 = 底朝上
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;        // 32 位每像素
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
                                    (void**)&pBits, nullptr, 0);
    if (!hBmp || !pBits) return;

    // 填充像素数据 —— 每个像素独立设置 Alpha
    for (int py = 0; py < height; py++)
    {
        for (int px = 0; px < width; px++)
        {
            float fx = (float)px / (float)(width - 1);

            // 从左到右: 紫色渐变
            BYTE blue  = 180;
            BYTE green = 60;
            BYTE red   = 220;
            // Alpha 从 0（完全透明）渐变到 255（完全不透明）
            BYTE alpha = (BYTE)(fx * 255.0f);

            // DIB 底朝上存储，翻转行号
            int row = height - 1 - py;
            int offset = (row * width + px) * 4;
            pBits[offset + 0] = blue;    // Blue
            pBits[offset + 1] = green;   // Green
            pBits[offset + 2] = red;     // Red
            pBits[offset + 3] = alpha;   // Alpha
        }
    }

    // 将 DIB 选入内存 DC
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    // 使用 AlphaBlend 绘制，启用逐像素 Alpha
    BLENDFUNCTION bf;
    bf.BlendOp             = AC_SRC_OVER;
    bf.BlendFlags          = 0;
    bf.SourceConstantAlpha = 255;          // 常量 Alpha 设为 255（不影响）
    bf.AlphaFormat         = AC_SRC_ALPHA; // 使用源位图中的逐像素 Alpha

    AlphaBlend(
        hdc, x, y, width, height,
        hdcMem, 0, 0, width, height,
        bf
    );

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}

/**
 * 绘制面板标题
 */
static void DrawPanelTitle(HDC hdc, const wchar_t* text, int x, int y, int width)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(30, 30, 30));

    HFONT hFont = CreateFont(
        14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    RECT rc = { x, y, x + width, y + 20 };
    DrawText(hdc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
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
        // --- 初始化 Common Controls ---
        // 使用 InitCommonControlsEx 注册 TrackBar 等公共控件类
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC  = ICC_BAR_CLASSES;  // 包含 TrackBar 类
        InitCommonControlsEx(&icc);

        // --- 创建 TrackBar 控件 ---
        // TRACKBAR_CLASS 定义在 commctrl.h 中
        HWND hTrackBar = CreateWindowEx(
            0,
            TRACKBAR_CLASS,                // 控件类名
            L"",                            // TrackBar 无标题
            WS_CHILD | WS_VISIBLE |
            TBS_HORZ | TBS_AUTOTICKS |     // 水平方向，自动刻度
            TBS_BOTTOM,                     // 刻度在底部
            160, WINDOW_HEIGHT - 80,        // 位置
            280, 35,                        // 尺寸
            hwnd,
            (HMENU)IDC_TRACKBAR,           // 控件 ID
            ((LPCREATESTRUCT)lParam)->hInstance,
            nullptr
        );

        if (hTrackBar)
        {
            // 设置 TrackBar 范围: 0（完全透明）到 255（完全不透明）
            SendMessage(hTrackBar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
            // 设置初始位置
            SendMessage(hTrackBar, TBM_SETPOS, TRUE, g_alphaValue);
            // 设置刻度频率
            SendMessage(hTrackBar, TBM_SETTICFREQ, 32, 0);
        }
        return 0;
    }

    case WM_HSCROLL:
    {
        // --- 处理 TrackBar 滑动事件 ---
        // WM_HSCROLL 消息由水平 TrackBar 发送
        HWND hTrackBar = (HWND)lParam;
        if (hTrackBar && GetDlgCtrlID(hTrackBar) == IDC_TRACKBAR)
        {
            // 获取当前滑块位置
            g_alphaValue = (int)SendMessage(hTrackBar, TBM_GETPOS, 0, 0);
            // 触发重绘
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
        int clientW = rcClient.right;
        int clientH = rcClient.bottom;

        // 填充白色背景
        HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(hdc, &rcClient, hWhite);

        int topY = 10;
        int panelGap = 12;

        // ============================================================
        // 面板 1: 常量 Alpha 混合 —— 三个重叠的半透明矩形
        // ============================================================
        {
            int px = 16, py = topY;
            int panelW = clientW - 32;
            int titleH = 22;

            DrawPanelTitle(hdc, L"常量 Alpha 混合 (SourceConstantAlpha)", px, py, panelW);
            py += titleH;

            // 绘制区域高度
            int drawH = std::min(200, (clientH - 140) / 2);

            // 先绘制棋盘格背景（展示透明效果）
            DrawCheckerboard(hdc, px, py, panelW, drawH, 12);

            // 绘制三个重叠的半透明矩形
            int drawSize = std::min(panelW, drawH) - 10;
            DrawOverlappingRects(hdc,
                px + (panelW - drawSize) / 2,
                py + (drawH - drawSize) / 2,
                drawSize, (BYTE)g_alphaValue);

            // 边框
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, px - 1, py - 1, px + panelW + 1, py + drawH + 1);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            topY = py + drawH + panelGap;
        }

        // ============================================================
        // 面板 2: 逐像素 Alpha 混合
        // ============================================================
        {
            int px = 16, py = topY;
            int panelW = clientW - 32;
            int titleH = 22;

            DrawPanelTitle(hdc, L"逐像素 Alpha 混合 (AC_SRC_ALPHA)", px, py, panelW);
            py += titleH;

            int drawH = std::min(120, (clientH - 140) / 2);

            // 先绘制棋盘格背景
            DrawCheckerboard(hdc, px, py, panelW, drawH, 12);

            // 绘制逐像素 Alpha 的渐变条
            int gradientH = drawH - 10;
            int gradientW = panelW - 20;
            DrawPerPixelAlpha(hdc, px + 10, py + 5, gradientW, gradientH);

            // 边框
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, px - 1, py - 1, px + panelW + 1, py + drawH + 1);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            // 在渐变条下方标注
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(100, 100, 100));
            HFONT hSmallFont = CreateFont(
                11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hSmallFont);

            RECT rcLeft = { px + 10, py + drawH - 16, px + 60, py + drawH - 2 };
            DrawText(hdc, L"Alpha=0 (透明)", -1, &rcLeft, DT_LEFT | DT_SINGLELINE);

            RECT rcRight = { px + panelW - 110, py + drawH - 16, px + panelW - 10, py + drawH - 2 };
            DrawText(hdc, L"Alpha=255 (不透明)", -1, &rcRight, DT_RIGHT | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            DeleteObject(hSmallFont);

            topY = py + drawH + panelGap;
        }

        // ============================================================
        // 底部: Alpha 值显示与 TrackBar 标注
        // ============================================================
        {
            // Alpha 值显示
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(30, 30, 30));
            HFONT hFont = CreateFont(
                14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            wchar_t szAlpha[64];
            _snwprintf(szAlpha, 64, L"Alpha 值: %d  (0x%02X)", g_alphaValue, g_alphaValue);
            TextOut(hdc, 16, WINDOW_HEIGHT - 78, szAlpha, (int)wcslen(szAlpha));

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            // TrackBar 两侧标注
            HFONT hSmallFont = CreateFont(
                11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont2 = (HFONT)SelectObject(hdc, hSmallFont);
            SetTextColor(hdc, RGB(100, 100, 100));

            TextOut(hdc, 100, WINDOW_HEIGHT - 60, L"0", 1);
            TextOut(hdc, 450, WINDOW_HEIGHT - 60, L"255", 3);

            // 绘制 Alpha 预览色块
            {
                int previewX = 480;
                int previewY = WINDOW_HEIGHT - 78;
                int previewSize = 30;

                // 棋盘格背景
                DrawCheckerboard(hdc, previewX, previewY, previewSize, previewSize, 6);

                // 半透明红色方块预览
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hBmp = CreateCompatibleBitmap(hdc, previewSize, previewSize);
                HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

                HBRUSH hBrush = CreateSolidBrush(RGB(255, 50, 50));
                RECT rcFill = { 0, 0, previewSize, previewSize };
                FillRect(hdcMem, &rcFill, hBrush);
                DeleteObject(hBrush);

                BLENDFUNCTION bf;
                bf.BlendOp             = AC_SRC_OVER;
                bf.BlendFlags          = 0;
                bf.SourceConstantAlpha = (BYTE)g_alphaValue;
                bf.AlphaFormat         = 0;

                AlphaBlend(hdc, previewX, previewY, previewSize, previewSize,
                           hdcMem, 0, 0, previewSize, previewSize, bf);

                SelectObject(hdcMem, hOldBmp);
                DeleteObject(hBmp);
                DeleteDC(hdcMem);

                // 边框
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, previewX - 1, previewY - 1,
                          previewX + previewSize + 1, previewY + previewSize + 1);
                SelectObject(hdc, hOldBrush);
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            }

            SelectObject(hdc, hOldFont2);
            DeleteObject(hSmallFont);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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
        L"GDI Alpha 混合示例",
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
