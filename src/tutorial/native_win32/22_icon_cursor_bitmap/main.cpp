// 22_icon_cursor_bitmap/main.cpp
// 图标 / 光标 / 位图资源示例
// 演示：LoadIcon、LoadCursor、CreateCompatibleBitmap、BitBlt 等资源加载与绘制
// 参考：tutorial/native_win32/14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md

#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#include <string>

// ==================== 控件 ID ====================
#define IDC_CURSOR_COMBO   1001
#define IDC_DRAW_BITMAP    1002
#define IDC_BITMAP_AREA    1003

// ==================== 光标选项 ====================
struct CursorEntry
{
    const wchar_t* name;   // 显示名称
    LPWSTR         id;     // 系统光标 ID
};

static const CursorEntry g_cursors[] =
{
    { L"Arrow (标准箭头)",   IDC_ARROW },
    { L"Cross (十字)",       IDC_CROSS },
    { L"Hand (手形)",        IDC_HAND  },
    { L"IBeam (文本)",       IDC_IBEAM },
    { L"Wait (等待)",        IDC_WAIT  },
};

static const int g_cursorCount = sizeof(g_cursors) / sizeof(g_cursors[0]);

// ==================== 全局变量 ====================
static HINSTANCE g_hInst       = nullptr;
static HICON     g_hIcon       = nullptr;
static HCURSOR   g_hCurCurrent = nullptr;
static HBITMAP   g_hBitmap     = nullptr;  // 内存中创建的位图
static HWND      g_hCombo      = nullptr;
static HWND      g_hBtnDraw    = nullptr;
static HWND      g_hBmpArea    = nullptr;  // 用于展示位图的静态控件

// ==================== 创建内存位图 ====================
// 在内存 DC 中绘制一个彩色渐变矩形 + 文字，无需外部 .bmp 文件
static HBITMAP CreateSampleBitmap(int width, int height)
{
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp  = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectBitmap(hdcMem, hBmp);

    // ---------- 绘制渐变背景 ----------
    for (int y = 0; y < height; ++y)
    {
        int r = (y * 255) / height;
        int g = 128;
        int b = 255 - r;
        HPEN hPen   = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN hOld   = SelectPen(hdcMem, hPen);
        MoveToEx(hdcMem, 0, y, nullptr);
        LineTo(hdcMem, width, y);
        SelectPen(hdcMem, hOld);
        DeletePen(hPen);
    }

    // ---------- 绘制白色矩形边框 ----------
    RECT rc = { 10, 10, width - 10, height - 10 };
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectBrush(hdcMem, hBrush);
    Rectangle(hdcMem, rc.left, rc.top, rc.right, rc.bottom);

    // ---------- 绘制文字 ----------
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    HFONT hFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"微软雅黑");
    HFONT hOldFont = SelectFont(hdcMem, hFont);
    const wchar_t* text = L"内存位图示例";
    RECT textRc = rc;
    DrawTextW(hdcMem, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectFont(hdcMem, hOldFont);
    DeleteFont(hFont);

    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hBmp;
}

// ==================== 窗口过程 ====================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // ---------- 设置光标 ----------
    // 当鼠标在客户区时，使用我们选择的光标
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT && g_hCurCurrent)
        {
            SetCursor(g_hCurCurrent);
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_CREATE:
    {
        // ---- ComboBox: 选择光标 ----
        g_hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL,
            20, 20, 220, 200,
            hwnd, (HMENU)IDC_CURSOR_COMBO, g_hInst, nullptr);

        // 填充选项
        for (int i = 0; i < g_cursorCount; ++i)
        {
            SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)g_cursors[i].name);
        }
        SendMessageW(g_hCombo, CB_SETCURSEL, 0, 0);  // 默认选中 Arrow

        // 默认光标为箭头
        g_hCurCurrent = LoadCursorW(nullptr, IDC_ARROW);

        // ---- 静态文字提示 ----
        CreateWindowExW(0, L"STATIC", L"选择光标样式：",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 2, 200, 18,
            hwnd, nullptr, g_hInst, nullptr);

        // ---- 按钮：绘制位图 ----
        g_hBtnDraw = CreateWindowExW(0, L"BUTTON", L"绘制位图",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            260, 20, 120, 30,
            hwnd, (HMENU)IDC_DRAW_BITMAP, g_hInst, nullptr);

        // ---- 静态控件作为位图展示区域 ----
        g_hBmpArea = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE | WS_BORDER,
            20, 65, 360, 200,
            hwnd, (HMENU)IDC_BITMAP_AREA, g_hInst, nullptr);

        // 创建内存位图 (360×200)
        g_hBitmap = CreateSampleBitmap(360, 200);
        // 预览：将位图设置到静态控件
        SendMessageW(g_hBmpArea, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_hBitmap);

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        // ---- 光标选择 ----
        case IDC_CURSOR_COMBO:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int sel = (int)SendMessageW(g_hCombo, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_cursorCount)
                {
                    g_hCurCurrent = LoadCursorW(nullptr, g_cursors[sel].id);
                    // 触发 WM_SETCURSOR 刷新
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        // ---- 绘制位图按钮 ----
        case IDC_DRAW_BITMAP:
        {
            // 使用 BitBlt 将位图绘制到窗口客户区
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectBitmap(hdcMem, g_hBitmap);

            // 获取静态控件区域，在其下方或直接 BitBlt
            RECT rc;
            GetClientRect(g_hBmpArea, &rc);
            MapWindowPoints(g_hBmpArea, hwnd, (POINT*)&rc, 2);

            BitBlt(hdc, rc.left, rc.top,
                   rc.right - rc.left, rc.bottom - rc.top,
                   hdcMem, 0, 0, SRCCOPY);

            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);

            // 也可以刷新静态控件
            InvalidateRect(g_hBmpArea, nullptr, FALSE);
            return 0;
        }
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 绘制提示信息
        SetBkMode(hdc, TRANSPARENT);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        rcClient.top = 275;
        DrawTextW(hdc, L"将鼠标移入窗口可查看选定的光标效果", -1, &rcClient,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
    {
        // 清理资源
        if (g_hBitmap)
            DeleteBitmap(g_hBitmap);
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== WinMain ====================
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    g_hInst = hInstance;

    // ---- 加载图标 ----
    // 尝试从资源加载自定义图标，失败则使用系统默认图标
    g_hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!g_hIcon)
    {
        // 后备方案：使用系统标准应用图标
        g_hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    // ---- 注册窗口类 ----
    const wchar_t CLASS_NAME[] = L"IconCursorBitmapDemo";

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = g_hIcon;                                    // 窗口图标
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);           // 默认光标（后续动态切换）
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm       = g_hIcon;                                    // 任务栏小图标

    RegisterClassExW(&wc);

    // ---- 创建主窗口 ----
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME,
        L"图标/光标/位图资源示例",    // 窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 350,                    // 窗口大小 500×350
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---- 消息循环 ----
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
