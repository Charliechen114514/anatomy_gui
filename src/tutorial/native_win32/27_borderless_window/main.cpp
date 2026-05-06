#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <cstdio>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HWND g_hWnd = NULL;
int  g_scale = 100;     // 缩放百分比

// 自定义命中测试
LRESULT CustomHitTest(HWND hwnd, int x, int y)
{
    const int BORDER = 6;
    const int CAPTION_HEIGHT = 40;

    RECT rc;
    GetWindowRect(hwnd, &rc);

    bool onLeft   = (x >= rc.left && x < rc.left + BORDER);
    bool onRight  = (x < rc.right && x >= rc.right - BORDER);
    bool onTop    = (y >= rc.top && y < rc.top + BORDER);
    bool onBottom = (y < rc.bottom && y >= rc.bottom - BORDER);

    if (onTop    && onLeft)  return HTTOPLEFT;
    if (onTop    && onRight) return HTTOPRIGHT;
    if (onBottom && onLeft)  return HTBOTTOMLEFT;
    if (onBottom && onRight) return HTBOTTOMRIGHT;
    if (onTop)    return HTTOP;
    if (onBottom) return HTBOTTOM;
    if (onLeft)   return HTLEFT;
    if (onRight)  return HTRIGHT;

    // 自定义标题栏区域
    if (y - rc.top < CAPTION_HEIGHT && !onLeft && !onRight)
        return HTCAPTION;

    return HTCLIENT;
}

void PaintContent(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    // 绘制标题栏背景
    RECT rcCaption = { 0, 0, rc.right, 40 };
    HBRUSH hBrushCaption = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rcCaption, hBrushCaption);
    DeleteObject(hBrushCaption);

    // 绘制标题文字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    wchar_t title[64];
    swprintf(title, 64, L"无边框窗口示例 - 缩放 %d%%", g_scale);
    TextOut(hdc, 12, 10, title, (int)wcslen(title));

    // 绘制关闭按钮区域
    RECT rcClose = { rc.right - 46, 0, rc.right, 40 };
    HBRUSH hBrushClose = CreateSolidBrush(RGB(232, 17, 35));
    FillRect(hdc, &rcClose, hBrushClose);
    DeleteObject(hBrushClose);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, rc.right - 32, 10, L"X", 1);

    // 绘制内容区域
    RECT rcContent = { 0, 40, rc.right, rc.bottom };
    HBRUSH hBrushContent = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &rcContent, hBrushContent);
    DeleteObject(hBrushContent);

    // 绘制示例内容（一个随缩放变化的圆）
    int radius = MulDiv(80, g_scale, 100);
    int cx = rc.right / 2;
    int cy = 40 + (rc.bottom - 40) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 212));
    HBRUSH hBrushCircle = CreateSolidBrush(RGB(0, 120, 212));
    SelectObject(hdc, hPen);
    SelectObject(hdc, hBrushCircle);

    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);

    DeleteObject(hPen);
    DeleteObject(hBrushCircle);

    // 提示文字
    SetTextColor(hdc, RGB(80, 80, 80));
    const wchar_t* hint = L"鼠标滚轮缩放 | 拖动顶部移动 | 拖动边缘缩放";
    TextOut(hdc, (rc.right - (int)wcslen(hint) * 8) / 2, rc.bottom - 30,
            hint, (int)wcslen(hint));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NCHITTEST:
        return CustomHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
        pmmi->ptMinTrackSize.x = 400;
        pmmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintContent(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int step = delta / WHEEL_DELTA;

        g_scale += step * 10;
        if (g_scale < 20)  g_scale = 20;
        if (g_scale > 500) g_scale = 500;

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 检查是否点击了关闭按钮区域
        POINTS pt = MAKEPOINTS(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (pt.y < 40 && pt.x >= rc.right - 46)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_DESTROY:
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
    wc.lpszClassName = L"BorderlessWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"BorderlessWindow", L"",
        WS_POPUP | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
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
