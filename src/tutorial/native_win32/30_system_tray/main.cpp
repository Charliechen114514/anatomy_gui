#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <shellapi.h>

#define WM_TRAYICON  (WM_APP + 100)
#define ID_TRAY_ICON 1

// 菜单命令
#define IDM_RESTORE  2001
#define IDM_ABOUT    2002
#define IDM_EXIT     2003

// 全局变量
UINT  g_uTaskbarRestart = 0;
BOOL  g_inTray = FALSE;
HWND  g_hWnd = NULL;

// 前向声明
BOOL AddTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
void ShowTrayContextMenu(HWND hwnd);

BOOL AddTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(nid.szTip, L"托盘示例 - 双击还原窗口");

    if (!Shell_NotifyIcon(NIM_ADD, &nid))
        return FALSE;

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    return TRUE;
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowTrayContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"还原窗口");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    SetForegroundWindow(hwnd);

    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 处理任务栏重启消息（不能用 switch）
    if (uMsg == g_uTaskbarRestart)
    {
        if (g_inTray)
            AddTrayIcon(hwnd);
        return 0;
    }

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建提示文本
        CreateWindowEx(0, L"STATIC",
            L"点击窗口关闭按钮最小化到系统托盘。\r\n\r\n"
            L"托盘图标右键菜单可还原或退出。\r\n"
            L"双击托盘图标还原窗口。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 340, 80,
            hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        return 0;
    }

    case WM_CLOSE:
        // 隐藏窗口并添加托盘图标
        if (!g_inTray)
        {
            AddTrayIcon(hwnd);
            g_inTray = TRUE;

            // 首次最小化时显示气泡提示
            NOTIFYICONDATA nid = {};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = ID_TRAY_ICON;
            nid.uFlags = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;
            wcscpy(nid.szInfoTitle, L"托盘示例");
            wcscpy(nid.szInfo, L"程序已最小化到系统托盘，双击图标可还原");
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_TRAYICON:
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case WM_RBUTTONUP:
            ShowTrayContextMenu(hwnd);
            break;
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_RESTORE:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case IDM_ABOUT:
            MessageBox(hwnd,
                L"系统托盘示例程序\r\n版本 1.0\r\n\r\n"
                L"演示 Shell_NotifyIcon 的用法",
                L"关于", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_EXIT:
            RemoveTrayIcon(hwnd);
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    g_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TrayDemoClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"TrayDemoClass", L"系统托盘示例",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 180,
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
