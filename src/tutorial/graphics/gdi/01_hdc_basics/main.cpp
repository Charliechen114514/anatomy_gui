/**
 * 01_hdc_basics - HDC 设备上下文示例
 *
 * 本程序演示了 Win32 GDI 中获取设备上下文 (HDC) 的四种方式：
 * 1. BeginPaint / EndPaint    — 在 WM_PAINT 中获取，仅用于响应重绘请求
 * 2. GetDC / ReleaseDC        — 在任意时刻获取客户区 DC
 * 3. GetWindowDC / ReleaseDC  — 获取整个窗口（含标题栏）的 DC
 * 4. CreateCompatibleDC       — 创建内存 DC，用于离屏渲染
 *
 * 同时演示了 SaveDC / RestoreDC 保存和恢复 DC 状态的用法。
 *
 * 参考: tutorial/native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// ============================================================================
// 常量与状态定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"HDCBasicsClass";

// "刷新" 按钮的子窗口 ID
static const int ID_BTN_REFRESH = 101;

/**
 * 应用程序状态结构体
 *
 * 存储在 GWLP_USERDATA 中，供窗口过程随时访问。
 * 这里保存了鼠标点击位置，用于 GetDC 方式的交互绘制。
 */
struct AppState
{
    POINT clickPos;         // 鼠标点击位置（用于 GetDC 交叉线绘制）
    bool  hasClick;         // 是否已经有了一次点击
};

// ============================================================================
// 辅助绘图函数
// ============================================================================

/**
 * 在指定矩形区域内绘制带标题的分区
 * 上方绘制标题栏，下方为演示区域
 */
static void DrawSection(HDC hdc, const RECT& rc, const wchar_t* title, COLORREF titleBg)
{
    // 保存当前 DC 状态
    int savedState = SaveDC(hdc);

    // 绘制标题背景
    HBRUSH hTitleBrush = CreateSolidBrush(titleBg);
    RECT titleRc = { rc.left, rc.top, rc.right, rc.top + 28 };
    FillRect(hdc, &titleRc, hTitleBrush);
    DeleteObject(hTitleBrush);

    // 绘制标题文字（白色）
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, title, -1, &titleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 绘制分区边框
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    DeleteObject(hBorderPen);

    // 恢复 DC 状态
    RestoreDC(hdc, savedState);
}

// ============================================================================
// 各分区的绘制函数
// ============================================================================

/**
 * 分区 1: BeginPaint / EndPaint (WM_PAINT)
 *
 * 这是在 WM_PAINT 消息中获取 HDC 的标准方式。
 * BeginPaint 会自动设置裁剪区域，EndPaint 会验证无效区域。
 * 只能在处理 WM_PAINT 时使用。
 */
static void DrawBeginPaintSection(HDC hdc, const RECT& rc)
{
    DrawSection(hdc, rc, L"1. BeginPaint / EndPaint", RGB(70, 130, 180));

    int savedState = SaveDC(hdc);

    // 在区域内绘制说明文字
    RECT textRc = { rc.left + 10, rc.top + 35, rc.right - 10, rc.top + 70 };
    SetTextColor(hdc, RGB(60, 60, 60));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"WM_PAINT 中自动调用", -1, &textRc,
             DT_LEFT | DT_WORDBREAK);

    // 绘制一个带背景色的 "WM_PAINT" 标签
    RECT labelRc = { rc.left + 20, rc.top + 75, rc.right - 20, rc.top + 115 };
    HBRUSH hLabelBg = CreateSolidBrush(RGB(220, 240, 255));
    FillRect(hdc, &labelRc, hLabelBg);
    DeleteObject(hLabelBg);

    SetTextColor(hdc, RGB(0, 70, 140));
    HFONT hLabelFont = CreateFont(
        24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hLabelFont);
    DrawText(hdc, L"WM_PAINT", -1, &labelRc,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hLabelFont);

    // 绘制小图标 — 绿色圆点表示"正在使用"
    HBRUSH hGreen = CreateSolidBrush(RGB(76, 175, 80));
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(76, 175, 80));
    SelectObject(hdc, hGreen);
    SelectObject(hdc, hPen);
    Ellipse(hdc, rc.left + 15, rc.top + 125, rc.left + 27, rc.top + 137);
    DeleteObject(hGreen);
    DeleteObject(hPen);

    SetTextColor(hdc, RGB(80, 80, 80));
    RECT statusRc = { rc.left + 32, rc.top + 122, rc.right - 10, rc.top + 142 };
    DrawText(hdc, L"此分区由 WM_PAINT 自动触发绘制", -1, &statusRc,
             DT_LEFT | DT_SINGLELINE);

    RestoreDC(hdc, savedState);
}

/**
 * 分区 2: GetDC / ReleaseDC (任意时刻)
 *
 * GetDC 可以在任意时刻获取客户区的设备上下文。
 * 本例中，鼠标点击时使用 GetDC 在点击位置绘制十字线。
 */
static void DrawGetDCSection(HDC hdc, const RECT& rc)
{
    DrawSection(hdc, rc, L"2. GetDC / ReleaseDC", RGB(156, 39, 176));

    int savedState = SaveDC(hdc);

    RECT textRc = { rc.left + 10, rc.top + 35, rc.right - 10, rc.top + 65 };
    SetTextColor(hdc, RGB(60, 60, 60));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"点击此区域绘制十字线", -1, &textRc,
             DT_LEFT | DT_WORDBREAK);

    // 绘制提示区域背景
    RECT hintRc = { rc.left + 10, rc.top + 70, rc.right - 10, rc.bottom - 20 };
    HBRUSH hHintBg = CreateSolidBrush(RGB(250, 245, 255));
    FillRect(hdc, &hintRc, hHintBg);
    DeleteObject(hHintBg);

    SetTextColor(hdc, RGB(120, 80, 140));
    RECT centerRc = { rc.left + 10, rc.top + 90, rc.right - 10, rc.bottom - 30 };
    DrawText(hdc, L"鼠标点击 -> GetDC()\n绘制十字线 -> ReleaseDC()",
             -1, &centerRc, DT_CENTER | DT_WORDBREAK);

    RestoreDC(hdc, savedState);
}

/**
 * 分区 3: GetWindowDC (整个窗口)
 *
 * GetWindowDC 获取整个窗口的设备上下文，包括标题栏和边框。
 * 可以在标题栏上绘图！这里绘制一个红色边框装饰。
 */
static void DrawWindowDCSection(HDC hdc, const RECT& rc)
{
    DrawSection(hdc, rc, L"3. GetWindowDC", RGB(211, 84, 0));

    int savedState = SaveDC(hdc);

    RECT textRc = { rc.left + 10, rc.top + 35, rc.right - 10, rc.top + 75 };
    SetTextColor(hdc, RGB(60, 60, 60));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"GetWindowDC 可绘制整个窗口\n包括标题栏！", -1, &textRc,
             DT_LEFT | DT_WORDBREAK);

    // 绘制一个模拟窗口示意图
    int boxLeft   = rc.left + 30;
    int boxTop    = rc.top + 85;
    int boxRight  = rc.right - 30;
    int boxBottom = rc.bottom - 25;

    // 标题栏
    HBRUSH hTitleBar = CreateSolidBrush(RGB(0, 120, 215));
    RECT tbRc = { boxLeft, boxTop, boxRight, boxTop + 22 };
    FillRect(hdc, &tbRc, hTitleBar);
    DeleteObject(hTitleBar);

    // 客户区
    HBRUSH hClient = CreateSolidBrush(RGB(255, 255, 255));
    RECT clRc = { boxLeft, boxTop + 22, boxRight, boxBottom };
    FillRect(hdc, &clRc, hClient);
    DeleteObject(hClient);

    // 红色边框装饰（模拟 GetWindowDC 可在标题栏绘图）
    HPEN hRedPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    SelectObject(hdc, hRedPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, boxLeft - 2, boxTop - 2, boxRight + 2, boxBottom + 2);
    DeleteObject(hRedPen);

    // 窗口标题文字
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT tbTextRc = { boxLeft + 5, boxTop, boxRight - 40, boxTop + 22 };
    DrawText(hdc, L"窗口标题栏", -1, &tbTextRc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RestoreDC(hdc, savedState);
}

/**
 * 分区 4: CreateCompatibleDC (内存 DC)
 *
 * CreateCompatibleDC 创建一个与指定 DC 兼容的内存设备上下文。
 * 通常配合位图使用，实现离屏渲染（先画到内存，再一次性拷贝到屏幕）。
 */
static void DrawMemoryDCSection(HDC hdc, const RECT& rc)
{
    DrawSection(hdc, rc, L"4. CreateCompatibleDC", RGB(0, 150, 136));

    int savedState = SaveDC(hdc);

    RECT textRc = { rc.left + 10, rc.top + 35, rc.right - 10, rc.top + 75 };
    SetTextColor(hdc, RGB(60, 60, 60));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"内存DC实现离屏渲染\n先绘制到内存，再 BitBlt 到屏幕",
             -1, &textRc, DT_LEFT | DT_WORDBREAK);

    // 演示内存 DC 的使用
    int bmpW = rc.right - rc.left - 40;
    int bmpH = rc.bottom - rc.top - 110;
    if (bmpW < 10) bmpW = 10;
    if (bmpH < 10) bmpH = 10;

    // 创建兼容位图
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, bmpW, bmpH);
    // 创建内存 DC
    HDC hMemDC = CreateCompatibleDC(hdc);
    // 选入位图
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBitmap);

    // 在内存 DC 中绘制内容
    HBRUSH hBgBrush = CreateSolidBrush(RGB(232, 245, 233));
    RECT fillRc = { 0, 0, bmpW, bmpH };
    FillRect(hMemDC, &fillRc, hBgBrush);
    DeleteObject(hBgBrush);

    // 绘制渐变色条
    for (int i = 0; i < bmpW && i < 200; ++i)
    {
        HPEN hGradPen = CreatePen(PS_SOLID, 1,
            RGB(0, (i * 200 / (bmpW > 0 ? bmpW : 1)) + 55, 150));
        SelectObject(hMemDC, hGradPen);
        MoveToEx(hMemDC, i, 5, nullptr);
        LineTo(hMemDC, i, bmpH - 5);
        DeleteObject(hGradPen);
    }

    // 绘制文字
    SetTextColor(hMemDC, RGB(255, 255, 255));
    SetBkMode(hMemDC, TRANSPARENT);
    HFONT hMemFont = CreateFont(
        14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hMemDC, hMemFont);
    RECT memTextRc = { 5, bmpH / 2 - 12, bmpW - 5, bmpH / 2 + 12 };
    DrawText(hMemDC, L"内存DC离屏渲染", -1, &memTextRc,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hMemFont);

    // 将内存 DC 的内容一次性拷贝到屏幕
    BitBlt(hdc, rc.left + 20, rc.top + 82, bmpW, bmpH,
           hMemDC, 0, 0, SRCCOPY);

    // 清理
    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);

    // 说明文字
    SetTextColor(hdc, RGB(0, 120, 100));
    RECT noteRc = { rc.left + 10, rc.bottom - 25, rc.right - 10, rc.bottom - 5 };
    DrawText(hdc, L"上方内容来自内存DC -> BitBlt", -1, &noteRc,
             DT_CENTER | DT_SINGLELINE);

    RestoreDC(hdc, savedState);
}

/**
 * 使用 GetDC 在鼠标点击位置绘制十字线
 */
static void DrawCrosshairAtClick(HWND hwnd, const AppState* state)
{
    if (!state->hasClick) return;

    HDC hdc = GetDC(hwnd);

    // 使用 SaveDC/RestoreDC 保护 DC 状态
    int savedState = SaveDC(hdc);

    RECT clientRc;
    GetClientRect(hwnd, &clientRc);

    // 计算分区2的范围（右上角）
    int midX = clientRc.right / 2;
    RECT section2Rc = { midX, 0, clientRc.right, clientRc.bottom / 2 };

    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(156, 39, 176));
    SelectObject(hdc, hPen);

    // 水平线
    MoveToEx(hdc, section2Rc.left + 5, state->clickPos.y, nullptr);
    LineTo(hdc, section2Rc.right - 5, state->clickPos.y);

    // 垂直线
    MoveToEx(hdc, state->clickPos.x, section2Rc.top + 30, nullptr);
    LineTo(hdc, state->clickPos.x, section2Rc.bottom - 5);

    DeleteObject(hPen);

    // 在交叉点旁标注坐标
    wchar_t coord[64];
    wsprintf(coord, L"(%d, %d)", state->clickPos.x, state->clickPos.y);
    SetTextColor(hdc, RGB(156, 39, 176));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, state->clickPos.x + 8, state->clickPos.y - 18, coord,
            (int)wcslen(coord));

    RestoreDC(hdc, savedState);
    ReleaseDC(hwnd, hdc);
}

/**
 * 使用 GetWindowDC 在窗口标题栏上绘制红色边框
 */
static void DrawTitleBarBorder(HWND hwnd)
{
    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) return;

    int savedState = SaveDC(hdc);

    RECT windowRc;
    GetWindowRect(hwnd, &windowRc);
    int w = windowRc.right - windowRc.left;
    int h = windowRc.bottom - windowRc.top;

    // 绘制红色边框装饰（围绕整个窗口，包括标题栏）
    HPEN hRedPen = CreatePen(PS_SOLID, 3, RGB(220, 50, 50));
    SelectObject(hdc, hRedPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 2, 2, w - 2, h - 2);
    DeleteObject(hRedPen);

    // 在标题栏上绘制标记文字
    SetTextColor(hdc, RGB(220, 50, 50));
    SetBkMode(hdc, TRANSPARENT);
    HFONT hSmallFont = CreateFont(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
    );
    SelectObject(hdc, hSmallFont);
    TextOut(hdc, w - 200, 6, L"GetWindowDC 绘制!", 15);
    DeleteObject(hSmallFont);

    RestoreDC(hdc, savedState);
    ReleaseDC(hwnd, hdc);
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
        // 初始化应用程序状态
        AppState* state = new AppState();
        state->hasClick = false;
        state->clickPos = { 0, 0 };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 创建 "刷新" 按钮
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        CreateWindowEx(
            0, L"BUTTON", L"刷新",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 80, 30,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_REFRESH,
            cs->hInstance, nullptr
        );
        return 0;
    }

    case WM_DESTROY:
    {
        // 释放状态
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_REFRESH)
        {
            // 刷新按钮 -> 触发重绘
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state) return 0;

        int x = (SHORT)LOWORD(lParam);
        int y = (SHORT)HIWORD(lParam);

        // 判断是否在分区2范围内（右上角）
        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int midX = clientRc.right / 2;

        if (x >= midX && y <= clientRc.bottom / 2)
        {
            state->clickPos.x = x;
            state->clickPos.y = y;
            state->hasClick = true;

            // 先重绘分区2的背景，再绘制十字线
            RECT section2Rc = { midX, 0, clientRc.right, clientRc.bottom / 2 };
            InvalidateRect(hwnd, &section2Rc, TRUE);

            // 延迟绘制十字线（在 WM_PAINT 之后）
            // 这里使用 GetDC 直接绘制
            DrawCrosshairAtClick(hwnd, state);
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 使用 SaveDC 保存初始状态
        int globalSaved = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int midX = clientRc.right / 2;
        int midY = clientRc.bottom / 2;

        // 填充背景
        HBRUSH hBg = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 定义四个分区
        RECT section1 = { 0,  0,  midX, midY };
        RECT section2 = { midX, 0,  clientRc.right, midY };
        RECT section3 = { 0,  midY, midX, clientRc.bottom };
        RECT section4 = { midX, midY, clientRc.right, clientRc.bottom };

        // 绘制四个分区
        DrawBeginPaintSection(hdc, section1);
        DrawGetDCSection(hdc, section2);
        DrawWindowDCSection(hdc, section3);
        DrawMemoryDCSection(hdc, section4);

        // 恢复初始 DC 状态
        RestoreDC(hdc, globalSaved);

        EndPaint(hwnd, &ps);

        // 绘制完成后，用 GetWindowDC 在标题栏画红色边框
        DrawTitleBarBorder(hwnd);

        // 如果有之前的点击，重新绘制十字线
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state && state->hasClick)
        {
            DrawCrosshairAtClick(hwnd, state);
        }

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
        0,                          // 扩展窗口样式
        WINDOW_CLASS_NAME,          // 窗口类名称
        L"HDC 设备上下文示例",      // 窗口标题
        WS_OVERLAPPEDWINDOW,        // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        700, 500,                   // 窗口大小
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
