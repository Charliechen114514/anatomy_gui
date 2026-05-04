/**
 * 03_shapes - GDI 图形绘制示例
 *
 * 本程序演示了 Win32 GDI 中常用的图形绘制 API：
 * 1. MoveToEx / LineTo       — 直线绘制，组成星形图案
 * 2. Rectangle / RoundRect   — 矩形和圆角矩形
 * 3. Ellipse                 — 椭圆和圆形（重叠效果）
 * 4. Polygon                 — 多边形（五边形和三角形）
 * 5. Arc / Pie / Chord       — 弧线、扇形和弓形
 * 6. Polyline                — 折线段
 *
 * 每种图形在自己的网格单元格内绘制，并标注使用的 API 名称。
 *
 * 参考: tutorial/native_win32/20_ProgrammingGUI_NativeWindows_GDI_Shapes.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <algorithm>
#include <cmath>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GDIShapesClass";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    int placeholder;     // 预留，未来可扩展交互功能
};

// ============================================================================
// 辅助绘图函数
// ============================================================================

/**
 * 绘制网格单元格，带标题和边框
 */
static void DrawCell(HDC hdc, const RECT& rc, const wchar_t* title, COLORREF accentColor)
{
    int saved = SaveDC(hdc);

    // 单元格背景
    HBRUSH hBg = CreateSolidBrush(RGB(252, 252, 252));
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    // 底部标题栏
    RECT titleRc = { rc.left, rc.bottom - 22, rc.right, rc.bottom };
    HBRUSH hTitleBg = CreateSolidBrush(accentColor);
    FillRect(hdc, &titleRc, hTitleBg);
    DeleteObject(hTitleBg);

    // 标题文字
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    HFONT hTitleFont = CreateFont(
        12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
    );
    SelectObject(hdc, hTitleFont);
    DrawText(hdc, title, -1, &titleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hTitleFont);

    // 单元格边框
    HPEN hBorder = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    SelectObject(hdc, hBorder);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    DeleteObject(hBorder);

    RestoreDC(hdc, saved);
}

/**
 * 获取单元格的绘图区域（去掉标题栏部分）
 */
static void GetCellDrawArea(const RECT& cell, RECT* outDrawRc)
{
    outDrawRc->left   = cell.left + 5;
    outDrawRc->top    = cell.top + 5;
    outDrawRc->right  = cell.right - 5;
    outDrawRc->bottom = cell.bottom - 25;
}

// ============================================================================
// 各图形绘制函数
// ============================================================================

/**
 * 单元格 1: MoveToEx / LineTo — 星形图案
 *
 * 用直线首尾相连绘制五角星。
 */
static void DrawStar(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"MoveToEx / LineTo", RGB(244, 67, 54));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int cx = (drawRc.left + drawRc.right) / 2;
    int cy = (drawRc.top + drawRc.bottom) / 2;
    int radius = std::min(drawRc.right - drawRc.left, drawRc.bottom - drawRc.top) / 2 - 5;
    if (radius < 10) radius = 10;

    // 计算五角星的 5 个外顶点和 5 个内顶点
    POINT pts[10];
    for (int i = 0; i < 5; ++i)
    {
        double outerAngle = -M_PI / 2 + i * 2 * M_PI / 5;
        pts[i * 2].x = cx + (int)(radius * cos(outerAngle));
        pts[i * 2].y = cy + (int)(radius * sin(outerAngle));

        double innerAngle = outerAngle + M_PI / 5;
        double innerR = radius * 0.382;
        pts[i * 2 + 1].x = cx + (int)(innerR * cos(innerAngle));
        pts[i * 2 + 1].y = cy + (int)(innerR * sin(innerAngle));
    }

    // 用 MoveToEx / LineTo 绘制
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(244, 67, 54));
    SelectObject(hdc, hPen);

    MoveToEx(hdc, pts[0].x, pts[0].y, nullptr);
    for (int i = 1; i < 10; ++i)
    {
        LineTo(hdc, pts[i].x, pts[i].y);
    }
    LineTo(hdc, pts[0].x, pts[0].y);

    DeleteObject(hPen);
    RestoreDC(hdc, saved);
}

/**
 * 单元格 2: Rectangle / RoundRect — 矩形和圆角矩形
 *
 * 绘制三个矩形：一个标准矩形和两个不同圆角半径的矩形。
 */
static void DrawRectangles(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"Rectangle / RoundRect", RGB(33, 150, 243));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int w = drawRc.right - drawRc.left;
    int h = drawRc.bottom - drawRc.top;
    int rw = w / 3 - 8;

    // 颜色组
    COLORREF colors[] = { RGB(33, 150, 243), RGB(100, 181, 246), RGB(187, 222, 251) };

    for (int i = 0; i < 3; ++i)
    {
        int x = drawRc.left + 6 + i * (rw + 8);
        int y = drawRc.top + 10;
        int bh = h - 20;

        HPEN hPen = CreatePen(PS_SOLID, 1, colors[i]);
        SelectObject(hdc, hPen);
        HBRUSH hBrush = CreateSolidBrush(colors[i]);
        SelectObject(hdc, hBrush);

        if (i == 0)
        {
            // 标准 Rectangle
            Rectangle(hdc, x, y, x + rw, y + bh);
        }
        else
        {
            // RoundRect — 不同的圆角半径
            int cornerW = 8 + i * 6;
            int cornerH = 8 + i * 6;
            RoundRect(hdc, x, y, x + rw, y + bh, cornerW, cornerH);
        }

        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    RestoreDC(hdc, saved);
}

/**
 * 单元格 3: Ellipse — 重叠椭圆
 *
 * 绘制三个部分重叠的半透明风格圆形。
 */
static void DrawEllipses(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"Ellipse", RGB(156, 39, 176));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int cx = (drawRc.left + drawRc.right) / 2;
    int cy = (drawRc.top + drawRc.bottom) / 2;
    int r = std::min(drawRc.right - drawRc.left, drawRc.bottom - drawRc.top) / 3;
    if (r < 15) r = 15;

    // 三个不同颜色的重叠圆形
    struct CircleDef { int dx, dy; COLORREF penColor; COLORREF brushColor; };
    CircleDef circles[] = {
        { -r / 2,  -r / 4,  RGB(156, 39, 176), RGB(225, 190, 231) },
        {  r / 2,  -r / 4,  RGB(33, 150, 243),  RGB(187, 222, 251) },
        {  0,       r / 3,  RGB(76, 175, 80),   RGB(200, 230, 201) },
    };

    for (int i = 0; i < 3; ++i)
    {
        int ex = cx + circles[i].dx;
        int ey = cy + circles[i].dy;

        HPEN hPen = CreatePen(PS_SOLID, 2, circles[i].penColor);
        SelectObject(hdc, hPen);
        HBRUSH hBrush = CreateSolidBrush(circles[i].brushColor);
        SelectObject(hdc, hBrush);

        Ellipse(hdc, ex - r, ey - r, ex + r, ey + r);

        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    RestoreDC(hdc, saved);
}

/**
 * 单元格 4: Polygon — 五边形和三角形
 *
 * 使用 Polygon 函数绘制两个多边形。
 */
static void DrawPolygons(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"Polygon", RGB(255, 152, 0));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int midX = (drawRc.left + drawRc.right) / 2;
    int midY = (drawRc.top + drawRc.bottom) / 2;
    int sz = std::min(midX - drawRc.left, drawRc.bottom - drawRc.top) / 2 - 8;
    if (sz < 15) sz = 15;

    // 五边形 (左侧)
    {
        int cx = (drawRc.left + midX) / 2;
        int cy = midY;
        POINT pts[5];
        for (int i = 0; i < 5; ++i)
        {
            double angle = -M_PI / 2 + i * 2 * M_PI / 5;
            pts[i].x = cx + (int)(sz * cos(angle));
            pts[i].y = cy + (int)(sz * sin(angle));
        }
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 152, 0));
        SelectObject(hdc, hPen);
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 224, 178));
        SelectObject(hdc, hBrush);
        Polygon(hdc, pts, 5);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    // 三角形 (右侧)
    {
        int cx = (midX + drawRc.right) / 2;
        int cy = midY;
        POINT pts[3] = {
            { cx, cy - sz },
            { cx - sz, cy + sz * 2 / 3 },
            { cx + sz, cy + sz * 2 / 3 },
        };
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(244, 67, 54));
        SelectObject(hdc, hPen);
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 205, 210));
        SelectObject(hdc, hBrush);
        Polygon(hdc, pts, 3);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    RestoreDC(hdc, saved);
}

/**
 * 单元格 5: Arc / Pie / Chord — 弧线、扇形和弓形
 *
 * 三种曲线图形：纯弧线、填充扇形、填充弓形。
 */
static void DrawArcs(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"Arc / Pie / Chord", RGB(0, 150, 136));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int w = drawRc.right - drawRc.left;
    int h = drawRc.bottom - drawRc.top;
    int partW = w / 3 - 4;
    int r = std::min(partW, h) / 2 - 5;
    if (r < 12) r = 12;

    struct ArcDef {
        const wchar_t* label;
        COLORREF color;
        COLORREF fillColor;
    };
    ArcDef defs[] = {
        { L"Arc",  RGB(0, 150, 136),  RGB(178, 223, 219) },
        { L"Pie",  RGB(244, 67, 54),  RGB(255, 205, 210) },
        { L"Chord", RGB(33, 150, 243), RGB(187, 222, 251) },
    };

    for (int i = 0; i < 3; ++i)
    {
        int cx = drawRc.left + 4 + i * (partW + 4) + partW / 2;
        int cy = drawRc.top + h / 2;

        int left   = cx - r;
        int top    = cy - r;
        int right  = cx + r;
        int bottom = cy + r;

        // 弧线的起止点角度（45度和315度，形成约270度的弧）
        int xStart = cx + (int)(r * cos(-M_PI / 4));
        int yStart = cy + (int)(r * sin(-M_PI / 4));
        int xEnd   = cx + (int)(r * cos(M_PI / 4 + M_PI / 2));
        int yEnd   = cy + (int)(r * sin(M_PI / 4 + M_PI / 2));

        HPEN hPen = CreatePen(PS_SOLID, 2, defs[i].color);
        SelectObject(hdc, hPen);
        HBRUSH hBrush = CreateSolidBrush(defs[i].fillColor);
        SelectObject(hdc, hBrush);

        switch (i)
        {
        case 0: Arc(hdc,   left, top, right, bottom, xStart, yStart, xEnd, yEnd); break;
        case 1: Pie(hdc,   left, top, right, bottom, xStart, yStart, xEnd, yEnd); break;
        case 2: Chord(hdc, left, top, right, bottom, xStart, yStart, xEnd, yEnd); break;
        }

        DeleteObject(hPen);
        DeleteObject(hBrush);

        // 标签
        SetTextColor(hdc, RGB(80, 80, 80));
        SetBkMode(hdc, TRANSPARENT);
        RECT labelRc = { cx - partW / 2, drawRc.bottom - 16, cx + partW / 2, drawRc.bottom };
        HFONT hSmall = CreateFont(
            11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        SelectObject(hdc, hSmall);
        DrawText(hdc, defs[i].label, -1, &labelRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(hSmall);
    }

    RestoreDC(hdc, saved);
}

/**
 * 单元格 6: Polyline — 折线段
 *
 * 使用 Polyline 绘制连接的折线段，模拟数据曲线。
 */
static void DrawPolyline(HDC hdc, const RECT& cell)
{
    DrawCell(hdc, cell, L"Polyline", RGB(103, 58, 183));

    int saved = SaveDC(hdc);
    RECT drawRc;
    GetCellDrawArea(cell, &drawRc);

    int w = drawRc.right - drawRc.left;
    int h = drawRc.bottom - drawRc.top;

    // 生成折线点（正弦波形状）
    int numPts = 20;
    POINT* pts = new POINT[numPts];
    for (int i = 0; i < numPts; ++i)
    {
        pts[i].x = drawRc.left + 10 + (int)((w - 20.0) * i / (numPts - 1));
        double t = (double)i / (numPts - 1);
        pts[i].y = drawRc.top + h / 2 - (int)(sin(t * 4 * M_PI) * (h / 2 - 15));
    }

    // 绘制参考网格线
    HPEN hGridPen = CreatePen(PS_DOT, 1, RGB(200, 200, 200));
    SelectObject(hdc, hGridPen);
    int midY = (drawRc.top + drawRc.bottom) / 2;
    MoveToEx(hdc, drawRc.left + 5, midY, nullptr);
    LineTo(hdc, drawRc.right - 5, midY);
    DeleteObject(hGridPen);

    // 绘制折线
    HPEN hLinePen = CreatePen(PS_SOLID, 2, RGB(103, 58, 183));
    SelectObject(hdc, hLinePen);
    Polyline(hdc, pts, numPts);
    DeleteObject(hLinePen);

    // 绘制数据点
    HBRUSH hDotBrush = CreateSolidBrush(RGB(103, 58, 183));
    SelectObject(hdc, hDotBrush);
    HPEN hDotPen = CreatePen(PS_SOLID, 1, RGB(103, 58, 183));
    SelectObject(hdc, hDotPen);
    for (int i = 0; i < numPts; ++i)
    {
        Ellipse(hdc, pts[i].x - 3, pts[i].y - 3, pts[i].x + 3, pts[i].y + 3);
    }
    DeleteObject(hDotBrush);
    DeleteObject(hDotPen);

    delete[] pts;
    RestoreDC(hdc, saved);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        AppState* state = new AppState();
        state->placeholder = 0;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;
    }

    case WM_DESTROY:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int saved = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充背景
        HBRUSH hBg = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 3 列 2 行网格
        int cols = 3;
        int rows = 2;
        int padX = 6, padY = 6;
        int cellW = (clientRc.right - padX * (cols + 1)) / cols;
        int cellH = (clientRc.bottom - padY * (rows + 1)) / rows;

        // 定义各单元格位置
        struct { int col; int row; } layout[] = {
            { 0, 0 },   // 星形
            { 1, 0 },   // 矩形
            { 2, 0 },   // 椭圆
            { 0, 1 },   // 多边形
            { 1, 1 },   // 弧线
            { 2, 1 },   // 折线
        };

        RECT cells[6];
        for (int i = 0; i < 6; ++i)
        {
            cells[i].left   = padX + layout[i].col * (cellW + padX);
            cells[i].top    = padY + layout[i].row * (cellH + padY);
            cells[i].right  = cells[i].left + cellW;
            cells[i].bottom = cells[i].top + cellH;
        }

        // 绘制各个图形
        DrawStar(hdc, cells[0]);
        DrawRectangles(hdc, cells[1]);
        DrawEllipses(hdc, cells[2]);
        DrawPolygons(hdc, cells[3]);
        DrawArcs(hdc, cells[4]);
        DrawPolyline(hdc, cells[5]);

        RestoreDC(hdc, saved);
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
        0,
        WINDOW_CLASS_NAME,
        L"GDI 图形绘制示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        700, 550,
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

    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
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
