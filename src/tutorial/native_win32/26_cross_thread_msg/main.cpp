#include <windows.h>
#include <commctrl.h>
#include <cstdio>

#define IDC_PROGRESS  1001
#define IDC_STATUS    1002
#define IDC_START_BTN 1003

// 自定义消息
#define WM_WORK_PROGRESS   (WM_APP + 1)
#define WM_WORK_COMPLETED  (WM_APP + 2)

// 全局变量
HWND g_hWnd = NULL;
HWND g_hProgress = NULL;
HWND g_hStatus = NULL;
HWND g_hBtnStart = NULL;
volatile LONG g_isRunning = 0;

// 工作线程函数
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    const int totalSteps = 100;

    for (int i = 1; i <= totalSteps; i++)
    {
        // 模拟耗时操作
        Sleep(50);

        // 通知主线程更新进度
        PostMessage(g_hWnd, WM_WORK_PROGRESS, i, totalSteps);
    }

    // 通知主线程完成
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, 0);

    InterlockedExchange(&g_isRunning, 0);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建进度条
        g_hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 20, 400, 25,
            hwnd, (HMENU)IDC_PROGRESS,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 创建状态文本
        g_hStatus = CreateWindowEx(0, L"STATIC", L"点击\"开始\"按钮启动计算",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 55, 400, 20,
            hwnd, (HMENU)IDC_STATUS,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 创建开始按钮
        g_hBtnStart = CreateWindowEx(0, L"BUTTON", L"开始计算",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 85, 120, 30,
            hwnd, (HMENU)IDC_START_BTN,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 设置进度条范围
        SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_START_BTN)
        {
            if (InterlockedCompareExchange(&g_isRunning, 1, 0) == 0)
            {
                // 禁用按钮，防止重复点击
                EnableWindow(g_hBtnStart, FALSE);
                SetWindowText(g_hStatus, L"计算中...");

                // 启动工作线程
                CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
            }
        }
        return 0;
    }

    case WM_WORK_PROGRESS:
    {
        int current = (int)wParam;
        int total = (int)lParam;

        // 更新进度条
        SendMessage(g_hProgress, PBM_SETPOS, current, 0);

        // 更新状态文本
        wchar_t buf[64];
        swprintf(buf, 64, L"进度：%d / %d (%.0f%%)",
            current, total, (double)current / total * 100.0);
        SetWindowText(g_hStatus, buf);

        return 0;
    }

    case WM_WORK_COMPLETED:
    {
        SetWindowText(g_hStatus, L"计算完成！");
        EnableWindow(g_hBtnStart, TRUE);
        MessageBox(hwnd, L"后台计算已完成", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
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
    // 初始化通用控件
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CrossThreadDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"CrossThreadDemo", L"跨线程通信示例",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 170,
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
