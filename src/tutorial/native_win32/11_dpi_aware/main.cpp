/**
 * 11_dpi_aware - DPI 感知示例（Per-Monitor V2）
 *
 * 演示 Windows Per-Monitor V2 DPI 感知：
 * - 声明 DPI 感知级别（Per-Monitor V2）
 * - 响应 WM_DPICHANGED 动态调整布局
 * - 使用 GetDpiForWindow 获取当前 DPI
 * - 使用 MulDiv 进行 DPI 缩放计算
 *
 * 参考: tutorial/native_win32/3_ProgrammingGUI_WhatAboutDPI.md
 */

#ifndef UNICODE
#define UNICODE
#endif

// 确保 Win10 API 可用
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#elif _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif

#include <windows.h>

#include <cmath>
#include <sstream>

// 动态加载 DPI 函数（兼容 MinGW / 旧 SDK）
typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
typedef BOOL(WINAPI* SetProcessDpiAwarenessContext_t)(HANDLE);

static GetDpiForWindow_t g_GetDpiForWindow = nullptr;
static SetProcessDpiAwarenessContext_t g_SetProcessDpiAwarenessContext = nullptr;

static void InitDpiFunctions()
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (!hUser32) hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32)
    {
        g_GetDpiForWindow = (GetDpiForWindow_t)GetProcAddress(hUser32, "GetDpiForWindow");
        g_SetProcessDpiAwarenessContext = (SetProcessDpiAwarenessContext_t)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    }
}

static const wchar_t* WINDOW_CLASS_NAME = L"DpiAwareClass";

// 控件 ID
#define ID_LABEL_DPI    101
#define ID_LABEL_SCALE  102
#define ID_BTN_REFRESH  103

// DPI 缩放辅助函数
int ScaleForDpi(int value, UINT dpi)
{
    return MulDiv(value, dpi, 96);
}

// 反向缩放（像素 → 逻辑单位）
int UnscaleForDpi(int value, UINT dpi)
{
    return MulDiv(value, 96, dpi);
}

// 获取窗口当前 DPI
UINT GetWindowDpi(HWND hwnd)
{
    if (g_GetDpiForWindow)
        return g_GetDpiForWindow(hwnd);
    // 回退：从 DC 获取 DPI
    HDC hdc = GetDC(hwnd);
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi ? dpi : 96;
}

// 主窗口状态
struct AppState
{
    HWND hLabelDpi;
    HWND hLabelScale;
    HWND hBtnRefresh;
    UINT currentDpi;
};

// 更新 DPI 信息显示
void UpdateDpiInfo(HWND hwnd, AppState* state)
{
    state->currentDpi = GetWindowDpi(hwnd);

    int scalePercent = MulDiv(100, state->currentDpi, 96);

    std::wostringstream oss;
    oss << L"当前 DPI: " << state->currentDpi
        << L"  |  缩放比例: " << scalePercent << L"%";
    SetWindowText(state->hLabelDpi, oss.str().c_str());

    std::wostringstream oss2;
    oss2 << L"100 逻辑像素 → " << ScaleForDpi(100, state->currentDpi)
         << L" 物理像素";
    SetWindowText(state->hLabelScale, oss2.str().c_str());
}

// 根据 DPI 创建控件
void CreateControls(HWND hwnd, AppState* state)
{
    UINT dpi = GetWindowDpi(hwnd);

    int margin = ScaleForDpi(15, dpi);
    int lineHeight = ScaleForDpi(30, dpi);
    int btnWidth = ScaleForDpi(120, dpi);
    int btnHeight = ScaleForDpi(32, dpi);

    // DPI 信息标签
    state->hLabelDpi = CreateWindowEx(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, margin,
        ScaleForDpi(500, dpi), lineHeight,
        hwnd, (HMENU)ID_LABEL_DPI,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);

    // 缩放示例标签
    state->hLabelScale = CreateWindowEx(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, margin + lineHeight + ScaleForDpi(5, dpi),
        ScaleForDpi(500, dpi), lineHeight,
        hwnd, (HMENU)ID_LABEL_SCALE,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);

    // 刷新按钮
    state->hBtnRefresh = CreateWindowEx(
        0, L"BUTTON", L"刷新 DPI",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, margin + (lineHeight + ScaleForDpi(5, dpi)) * 2,
        btnWidth, btnHeight,
        hwnd, (HMENU)ID_BTN_REFRESH,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);

    UpdateDpiInfo(hwnd, state);
}

// 重新布局控件（DPI 变化时调用）
void RelayoutControls(HWND hwnd, AppState* state, UINT newDpi)
{
    // 更新字体以匹配新 DPI
    HFONT hFont = CreateFont(
        -ScaleForDpi(14, newDpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    SendMessage(state->hLabelDpi, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(state->hLabelScale, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(state->hBtnRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 重新定位控件
    int margin = ScaleForDpi(15, newDpi);
    int lineHeight = ScaleForDpi(30, newDpi);
    int btnWidth = ScaleForDpi(120, newDpi);
    int btnHeight = ScaleForDpi(32, newDpi);

    MoveWindow(state->hLabelDpi,
        margin, margin,
        ScaleForDpi(500, newDpi), lineHeight, TRUE);

    MoveWindow(state->hLabelScale,
        margin, margin + lineHeight + ScaleForDpi(5, newDpi),
        ScaleForDpi(500, newDpi), lineHeight, TRUE);

    MoveWindow(state->hBtnRefresh,
        margin, margin + (lineHeight + ScaleForDpi(5, newDpi)) * 2,
        btnWidth, btnHeight, TRUE);

    UpdateDpiInfo(hwnd, state);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCT*)lParam;
        state = new AppState{};
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        CreateControls(hwnd, state);
        return 0;
    }

    case WM_DPICHANGED:
    {
        // wParam 的新 DPI 信息
        UINT newDpiX = LOWORD(wParam);
        UINT newDpiY = HIWORD(wParam);

        // lParam 包含建议的新窗口矩形
        auto* pRect = (RECT*)lParam;
        SetWindowPos(hwnd, nullptr,
            pRect->left, pRect->top,
            pRect->right - pRect->left,
            pRect->bottom - pRect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);

        RelayoutControls(hwnd, state, newDpiX);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_REFRESH)
        {
            UpdateDpiInfo(hwnd, state);
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 绘制 DPI 感知的矩形
        UINT dpi = GetWindowDpi(hwnd);
        int x = ScaleForDpi(15, dpi);
        int y = ScaleForDpi(140, dpi);
        int w = ScaleForDpi(200, dpi);
        int h = ScaleForDpi(80, dpi);

        // 绘制两个不同大小的矩形对比
        HPEN hPen = CreatePen(PS_SOLID, ScaleForDpi(2, dpi), RGB(0, 120, 215));
        HBRUSH hBrush = CreateSolidBrush(RGB(220, 235, 252));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

        Rectangle(hdc, x, y, x + w, y + h);

        // 绘制一个始终 100×100 物理像素的矩形（不缩放）作对比
        HPEN hPenGray = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        SelectObject(hdc, hPenGray);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x + w + ScaleForDpi(20, dpi), y, x + w + ScaleForDpi(20, dpi) + 100, y + 100);

        std::wstring label1 = L"DPI 缩放矩形";
        std::wstring label2 = L"100×100 物理像素";

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, x, y + h + ScaleForDpi(5, dpi), label1.c_str(), (int)label1.length());
        TextOut(hdc, x + w + ScaleForDpi(20, dpi), y + 100 + ScaleForDpi(5, dpi),
            label2.c_str(), (int)label2.length());

        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
        DeleteObject(hBrush);
        DeleteObject(hPenGray);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        delete state;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 初始化 DPI 函数指针
    InitDpiFunctions();

    // 尝试设置 Per-Monitor V2 DPI 感知
    if (g_SetProcessDpiAwarenessContext)
    {
        g_SetProcessDpiAwarenessContext((HANDLE)-4);
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, WINDOW_CLASS_NAME,
        L"DPI 感知示例 (Per-Monitor V2)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 350,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
