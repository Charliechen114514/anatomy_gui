/**
 * 01_gdiplus_hello - GDI+ 入门示例
 *
 * 本程序演示了 GDI+ 的基本使用方法，并与传统 GDI 进行对比：
 * 1. GdiplusStartup / GdiplusShutdown 初始化与清理
 * 2. Gdiplus::Graphics 对象 — 从 HDC 创建
 * 3. Gdiplus::SolidBrush, Gdiplus::Pen, Gdiplus::GraphicsPath
 * 4. SetSmoothingMode(SmoothingModeAntiAlias) — 抗锯齿绘制
 * 5. 左侧使用传统 GDI（锯齿明显），右侧使用 GDI+（平滑抗锯齿）
 * 6. 绘制填充圆形、弧线和抗锯齿文字
 *
 * 参考: tutorial/native_win32/26_ProgrammingGUI_Graphics_GdiPlus_Architecture.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#pragma comment(lib, "gdiplus.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GdiPlusHelloClass";

// ============================================================================
// 辅助函数：获取字符编码标识符
// ============================================================================

/**
 * GDI+ FontFamily 构造函数接受字符集标识符。
 * 这里辅助获取默认字体族名称。
 */

// ============================================================================
// 绘制左侧 GDI 传统绘制区域
// ============================================================================

/**
 * 使用传统 GDI 绘制 — 没有 GDI+ 的抗锯齿支持，锯齿明显
 */
static void DrawGDISection(HDC hdc, const RECT& rc)
{
    int savedState = SaveDC(hdc);

    // 绘制标题背景
    HBRUSH hTitleBrush = CreateSolidBrush(RGB(70, 130, 180));
    RECT titleRc = { rc.left, rc.top, rc.right, rc.top + 30 };
    FillRect(hdc, &titleRc, hTitleBrush);
    DeleteObject(hTitleBrush);

    // 标题文字
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    HFONT hTitleFont = CreateFont(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hTitleFont);
    DrawText(hdc, L"传统 GDI (无抗锯齿)", -1, &titleRc,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hTitleFont);

    // 填充分区背景
    int contentTop = rc.top + 32;
    RECT contentRc = { rc.left, contentTop, rc.right, rc.bottom };
    HBRUSH hBg = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &contentRc, hBg);
    DeleteObject(hBg);

    // 绘制矩形 — 传统 GDI，边缘有锯齿
    HPEN hBluePen = CreatePen(PS_SOLID, 3, RGB(0, 120, 215));
    SelectObject(hdc, hBluePen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left + 20, contentTop + 15, rc.left + 150, contentTop + 80);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hBluePen);

    // 绘制椭圆 — 传统 GDI
    HPEN hRedPen = CreatePen(PS_SOLID, 3, RGB(220, 50, 50));
    SelectObject(hdc, hRedPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, rc.left + 30, contentTop + 90, rc.left + 140, contentTop + 180);
    DeleteObject(hRedPen);

    // 绘制弧线 — 传统 GDI Arc
    HPEN hGreenPen = CreatePen(PS_SOLID, 3, RGB(76, 175, 80));
    SelectObject(hdc, hGreenPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Arc(hdc, rc.left + 20, contentTop + 185, rc.left + 150, contentTop + 260,
        rc.left + 150, contentTop + 220, rc.left + 20, contentTop + 230);
    DeleteObject(hGreenPen);

    // 绘制文字 — 传统 GDI TextOut
    HFONT hTextFont = CreateFont(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hTextFont);
    SetTextColor(hdc, RGB(100, 100, 100));
    TextOut(hdc, rc.left + 20, contentTop + 270, L"GDI 文字", 6);
    DeleteObject(hTextFont);

    // 边框
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(hdc, hBorderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    DeleteObject(hBorderPen);

    RestoreDC(hdc, savedState);
}

// ============================================================================
// 绘制右侧 GDI+ 抗锯齿绘制区域
// ============================================================================

/**
 * 使用 GDI+ 绘制 — 开启抗锯齿，边缘平滑
 */
static void DrawGdiPlusSection(HDC hdc, const RECT& rc)
{
    using namespace Gdiplus;

    // 创建 Graphics 对象
    Graphics graphics(hdc);

    // 开启抗锯齿模式 — 这是 GDI+ 的核心优势之一
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    // 绘制标题背景
    SolidBrush titleBrush(Color(255, 46, 139, 87));   // 绿色标题栏
    graphics.FillRectangle(&titleBrush, rc.left, rc.top,
                           rc.right - rc.left, 30);

    // 标题文字
    FontFamily fontFamily(L"微软雅黑");
    Font titleFont(&fontFamily, 14, FontStyleBold, UnitPixel);
    SolidBrush whiteBrush(Color(255, 255, 255, 255));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF titleRect((REAL)rc.left, (REAL)rc.top,
                    (REAL)(rc.right - rc.left), 30.0f);
    graphics.DrawString(L"GDI+ (抗锯齿)", -1, &titleFont,
                        titleRect, &sf, &whiteBrush);

    // 内容区域背景
    int contentTop = rc.top + 32;
    SolidBrush bgBrush(Color(255, 245, 245, 245));
    graphics.FillRectangle(&bgBrush, rc.left, contentTop,
                           rc.right - rc.left, rc.bottom - contentTop);

    // 绘制矩形 — GDI+ 抗锯齿
    Pen bluePen(Color(255, 0, 120, 215), 3.0f);
    graphics.DrawRectangle(&bluePen, rc.left + 20, contentTop + 15,
                           130, 65);

    // 绘制填充圆形 — GDI+ SolidBrush
    SolidBrush fillBrush(Color(200, 220, 50, 50));    // 半透明红色填充
    graphics.FillEllipse(&fillBrush, rc.left + 35, contentTop + 95,
                         100, 80);
    Pen redPen(Color(255, 220, 50, 50), 3.0f);
    graphics.DrawEllipse(&redPen, rc.left + 35, contentTop + 95,
                         100, 80);

    // 绘制弧线 — GDI+ DrawArc
    Pen greenPen(Color(255, 76, 175, 80), 3.0f);
    graphics.DrawArc(&greenPen, rc.left + 20, contentTop + 190,
                     130, 65, 180.0f, 180.0f);

    // 绘制抗锯齿文字 — GDI+ DrawString
    Font textFont(&fontFamily, 18, FontStyleBold, UnitPixel);
    SolidBrush textBrush(Color(255, 100, 100, 100));
    graphics.DrawString(L"GDI+ 文字", -1, &textFont,
                        PointF((REAL)(rc.left + 20), (REAL)(contentTop + 275)),
                        &textBrush);

    // 使用 GraphicsPath 绘制一个星形路径
    GraphicsPath starPath;
    REAL cx = (REAL)(rc.left + 85);
    REAL cy = (REAL)(contentTop + 360);
    REAL outerR = 25.0f;
    REAL innerR = 10.0f;
    for (int i = 0; i < 5; i++)
    {
        // 外部顶点
        REAL outerAngle = (REAL)(-90 + i * 72) * 3.14159265f / 180.0f;
        REAL ox = cx + outerR * cosf(outerAngle);
        REAL oy = cy + outerR * sinf(outerAngle);
        // 内部顶点
        REAL innerAngle = (REAL)(-90 + i * 72 + 36) * 3.14159265f / 180.0f;
        REAL ix = cx + innerR * cosf(innerAngle);
        REAL iy = cy + innerR * sinf(innerAngle);

        if (i == 0)
            starPath.StartFigure();
        else
            starPath.StartFigure();

        // 画到外部顶点
        if (i == 0)
            starPath.AddLine(cx, cy - outerR, ix, iy);
        else
        {
            REAL prevOuterAngle = (REAL)(-90 + (i - 1) * 72 + 36) * 3.14159265f / 180.0f;
            starPath.AddLine(cx + innerR * cosf(prevOuterAngle),
                             cy + innerR * sinf(prevOuterAngle),
                             ox, oy);
            starPath.AddLine(ox, oy, ix, iy);
        }

        // 连接到下一个外部顶点
        REAL nextOuterAngle = (REAL)(-90 + (i + 1) * 72) * 3.14159265f / 180.0f;
        if (i < 4)
            starPath.AddLine(ix, iy,
                             cx + outerR * cosf(nextOuterAngle),
                             cy + outerR * sinf(nextOuterAngle));
        else
            starPath.AddLine(ix, iy, cx, cy - outerR);
    }
    starPath.CloseFigure();

    SolidBrush starBrush(Color(255, 255, 193, 7));    // 金色填充
    Pen starPen(Color(255, 255, 152, 0), 2.0f);       // 橙色边框
    graphics.FillPath(&starBrush, &starPath);
    graphics.DrawPath(&starPen, &starPath);

    // 星形旁标注文字
    Font smallFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    SolidBrush noteBrush(Color(255, 150, 150, 150));
    graphics.DrawString(L"GraphicsPath 星形", -1, &smallFont,
                        PointF((REAL)(rc.left + 115), (REAL)(contentTop + 352)),
                        &noteBrush);

    // 边框
    Pen borderPen(Color(255, 180, 180, 180), 1.0f);
    graphics.DrawRectangle(&borderPen,
                           (REAL)rc.left, (REAL)rc.top,
                           (REAL)(rc.right - rc.left - 1), (REAL)(rc.bottom - rc.top - 1));
}

// ============================================================================
// 绘制底部说明文字
// ============================================================================

static void DrawBottomNote(HDC hdc, const RECT& rc)
{
    using namespace Gdiplus;

    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

    FontFamily fontFamily(L"微软雅黑");
    Font noteFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    SolidBrush noteBrush(Color(255, 120, 120, 120));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF noteRect((REAL)rc.left, (REAL)rc.top,
                   (REAL)(rc.right - rc.left), (REAL)(rc.bottom - rc.top));
    graphics.DrawString(
        L"GDI+ 通过 SetSmoothingMode(SmoothingModeAntiAlias) 实现抗锯齿绘制，"
        L"对比左侧传统 GDI 的锯齿效果，右侧 GDI+ 的图形和文字明显更加平滑。",
        -1, &noteFont, noteRect, &sf, &noteBrush);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充整体背景
        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 计算左右分区
        int midX = clientRc.right / 2;
        int bottomNoteHeight = 50;

        RECT leftRc  = { 5, 5, midX - 5, clientRc.bottom - bottomNoteHeight };
        RECT rightRc = { midX + 5, 5, clientRc.right - 5, clientRc.bottom - bottomNoteHeight };
        RECT noteRc  = { 10, clientRc.bottom - bottomNoteHeight,
                         clientRc.right - 10, clientRc.bottom - 5 };

        // 左侧: 传统 GDI 绘制（无抗锯齿）
        DrawGDISection(hdc, leftRc);

        // 右侧: GDI+ 绘制（抗锯齿）
        DrawGdiPlusSection(hdc, rightRc);

        // 底部说明
        DrawBottomNote(hdc, noteRc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 创建主窗口
// ============================================================================

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"GDI+ 入门示例",               // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        650, 400,                       // 窗口大小
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr)
        return nullptr;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // ---- GDI+ 初始化 ----
    // GdiplusStartup 必须在任何 GDI+ 操作之前调用
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // ---- GDI+ 清理 ----
    // GdiplusShutdown 必须在所有 GDI+ 操作完成后调用
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
