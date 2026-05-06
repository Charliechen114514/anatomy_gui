#include <windows.h>
#include <stdio.h>

HHOOK g_hHook = NULL;
HWND  g_hWnd = NULL;
BOOL  g_running = TRUE;

// 低级键盘钩子回调
LRESULT CALLBACK LowLevelKeyboardProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;

        // 检测 Print Screen 键按下
        if (pKbd->vkCode == VK_SNAPSHOT &&
            (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
        {
            // 通知主窗口
            PostMessage(g_hWnd, WM_USER + 1, 0, 0);

            // 拦截此按键（不让系统截图）
            return 1;
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Hook 线程（低级钩子需要消息循环）
DWORD WINAPI HookThread(LPVOID lpParam)
{
    // 安装低级键盘钩子
    g_hHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0  // 0 = 全局
    );

    if (!g_hHook)
    {
        MessageBox(NULL, L"安装 Hook 失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 低级钩子需要消息循环才能工作
    MSG msg = {};
    while (g_running && GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 移除 Hook
    UnhookWindowsHookEx(g_hHook);
    g_hHook = NULL;

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_USER + 1:
        MessageBox(hwnd,
            L"检测到 Print Screen 按键，已被拦截。\n"
            L"此示例仅用于教学目的。",
            L"Hook 通知", MB_OK | MB_ICONINFORMATION);
        return 0;

    case WM_DESTROY:
        g_running = FALSE;
        // 向 Hook 线程发送消息以退出消息循环
        PostThreadMessage(GetWindowThreadProcessId(hwnd, NULL), WM_QUIT, 0, 0);
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
    wc.lpszClassName = L"HookDemoClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"HookDemoClass",
        L"Hook 示例 - Print Screen 监听",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return 0;

    // 显示提示信息
    CreateWindowEx(0, L"STATIC",
        L"程序正在监听 Print Screen 键。\r\n\r\n"
        L"按下 Print Screen 后按键会被拦截，\r\n"
        L"同时弹出通知对话框。\r\n\r\n"
        L"关闭窗口退出程序。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 350, 140,
        g_hWnd, NULL, hInstance, NULL);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 启动 Hook 线程
    CreateThread(NULL, 0, HookThread, NULL, 0, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
