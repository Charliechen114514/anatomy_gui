#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <math.h>
#include <cstdio>

#define ID_CLOCK_TIMER 1
#define TIMER_INTERVAL 1000  // 1 秒

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 全局变量
HWND  g_hWnd = NULL;
SIZE  g_clientSize = { 400, 400 };

// 将角度（以 12 点为 0，顺时针）转换为弧度
double ClockToRadian(int hours, int minutes, int seconds)
{
    double totalSeconds = hours * 3600.0 + minutes * 60.0 + seconds;
    double fraction = totalSeconds / 43200.0;  // 12 小时 = 43200 秒
    return fraction * 2.0 * M_PI - M_PI / 2.0;
}

void DrawClock(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    int cx = rc.right / 2;
    int cy = rc.bottom / 2;
    int radius = (cx < cy ? cx : cy) - 20;

    // 背景
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // 表盘外圈
    HPEN hPenBorder = CreatePen(PS_SOLID, 3, RGB(60, 60, 60));
    SelectObject(hdc, hPenBorder);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    DeleteObject(hPenBorder);

    // 刻度
    for (int i = 0; i < 60; i++)
    {
        double angle = (i / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int innerR = (i % 5 == 0) ? radius - 20 : radius - 10;
        int outerR = radius - 4;

        int x1 = cx + (int)(innerR * cos(angle));
        int y1 = cy + (int)(innerR * sin(angle));
        int x2 = cx + (int)(outerR * cos(angle));
        int y2 = cy + (int)(outerR * sin(angle));

        if (i % 5 == 0)
        {
            HPEN hPenTick = CreatePen(PS_SOLID, 2, RGB(40, 40, 40));
            SelectObject(hdc, hPenTick);
            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y2);
            DeleteObject(hPenTick);
        }
        else
        {
            HPEN hPenTick = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
            SelectObject(hdc, hPenTick);
            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y2);
            DeleteObject(hPenTick);
        }
    }

    // 获取当前时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    // 时针
    {
        double angle = ClockToRadian(st.wHour, st.wMinute, st.wMinute * 60);
        int len = radius * 55 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 6, RGB(30, 30, 30));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 分针
    {
        double angle = ClockToRadian(st.wMinute, st.wSecond / 60.0, 0);
        // 分针角度 = 分钟 + 秒/60
        double totalMinutes = st.wMinute + st.wSecond / 60.0;
        angle = (totalMinutes / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int len = radius * 70 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 4, RGB(50, 50, 50));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 秒针
    {
        double angle = (st.wSecond / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int len = radius * 80 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        // 秒针尾部
        int tailLen = radius * 15 / 100;
        int xTail = cx - (int)(tailLen * cos(angle));
        int yTail = cy - (int)(tailLen * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 30, 30));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, xTail, yTail, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 中心点
    HBRUSH hBrush = CreateSolidBrush(RGB(200, 30, 30));
    SelectObject(hdc, hBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - 5, cy - 5, cx + 5, cy + 5);
    DeleteObject(hBrush);

    // 数字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(40, 40, 40));
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFont);

    for (int i = 1; i <= 12; i++)
    {
        double angle = (i / 12.0) * 2.0 * M_PI - M_PI / 2.0;
        int numR = radius - 35;
        int nx = cx + (int)(numR * cos(angle));
        int ny = cy + (int)(numR * sin(angle));

        wchar_t buf[8];
        swprintf(buf, 8, L"%d", i);

        SIZE sz;
        GetTextExtentPoint32(hdc, buf, (int)wcslen(buf), &sz);
        TextOut(hdc, nx - sz.cx / 2, ny - sz.cy / 2, buf, (int)wcslen(buf));
    }

    DeleteObject(hFont);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 启动定时器，每秒触发一次
        SetTimer(hwnd, ID_CLOCK_TIMER, TIMER_INTERVAL, NULL);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_CLOCK_TIMER)
            InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawClock(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
        pmmi->ptMinTrackSize.x = 300;
        pmmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_CLOCK_TIMER);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AnalogClock";
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"AnalogClock", L"模拟时钟 - WM_TIMER 示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 440,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWnd)
    {
        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
