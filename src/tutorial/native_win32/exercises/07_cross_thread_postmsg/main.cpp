// 练习7: 跨线程 PostMessage 进度条
// 提示：工作线程计算完成后通过 PostMessage 通知主线程更新进度条
// 扩展：尝试添加一个"取消"按钮，通过 Event 或 volatile 标志位通知工作线程提前退出

#include <windows.h>
#include <commctrl.h>
#include <cstdio>

#define IDC_PROGRESS   1001
#define IDC_START_BTN  1002
#define IDC_CANCEL_BTN 1003
#define IDC_STATUS     1004

// 自定义消息
#define WM_WORK_PROGRESS   (WM_APP + 1)
#define WM_WORK_COMPLETED  (WM_APP + 2)

// 全局变量
HWND g_hWnd = NULL;
HWND g_hProgress = NULL;
HWND g_hBtnStart = NULL;
HWND g_hBtnCancel = NULL;
HWND g_hStatus = NULL;
volatile LONG g_isRunning = 0;
volatile LONG g_cancelFlag = 0;

// 工作线程函数
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    const int totalSteps = 50;

    for (int i = 1; i <= totalSteps; i++)
    {
        // 检查是否被取消
        if (InterlockedCompareExchange(&g_cancelFlag, 0, 0) != 0)
        {
            // 已取消
            PostMessage(g_hWnd, WM_WORK_COMPLETED, 1, 0);  // wParam=1 表示取消
            InterlockedExchange(&g_isRunning, 0);
            return 0;
        }

        // 模拟耗时操作
        Sleep(80);

        // 通知主线程更新进度
        PostMessage(g_hWnd, WM_WORK_PROGRESS, i, totalSteps);
    }

    // 通知主线程完成
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, 0);  // wParam=0 表示正常完成

    InterlockedExchange(&g_isRunning, 0);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 创建进度条
        g_hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 20, 360, 25,
            hwnd, (HMENU)IDC_PROGRESS, hInst, NULL);

        // 创建状态文本
        g_hStatus = CreateWindowEx(0, L"STATIC", L"就绪",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 55, 360, 20,
            hwnd, (HMENU)IDC_STATUS, hInst, NULL);

        // 创建开始按钮
        g_hBtnStart = CreateWindowEx(0, L"BUTTON", L"开始",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 85, 80, 30,
            hwnd, (HMENU)IDC_START_BTN, hInst, NULL);

        // 创建取消按钮
        g_hBtnCancel = CreateWindowEx(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            200, 85, 80, 30,
            hwnd, (HMENU)IDC_CANCEL_BTN, hInst, NULL);

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
                InterlockedExchange(&g_cancelFlag, 0);
                EnableWindow(g_hBtnStart, FALSE);
                EnableWindow(g_hBtnCancel, TRUE);
                SetWindowText(g_hStatus, L"计算中...");
                SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
                CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
            }
        }
        else if (LOWORD(wParam) == IDC_CANCEL_BTN)
        {
            InterlockedExchange(&g_cancelFlag, 1);
        }
        return 0;
    }

    case WM_WORK_PROGRESS:
    {
        int current = (int)wParam;
        int total = (int)lParam;

        SendMessage(g_hProgress, PBM_SETPOS, current * 100 / total, 0);

        wchar_t buf[64];
        swprintf(buf, 64, L"进度：%d / %d (%.0f%%)",
            current, total, (double)current / total * 100.0);
        SetWindowText(g_hStatus, buf);
        return 0;
    }

    case WM_WORK_COMPLETED:
    {
        if (wParam == 1)
            SetWindowText(g_hStatus, L"已取消");
        else
            SetWindowText(g_hStatus, L"完成！");

        EnableWindow(g_hBtnStart, TRUE);
        EnableWindow(g_hBtnCancel, FALSE);
        return 0;
    }

    case WM_DESTROY:
        if (InterlockedCompareExchange(&g_isRunning, 0, 0) != 0)
        {
            InterlockedExchange(&g_cancelFlag, 1);
            Sleep(100);
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CrossThreadExercise";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"CrossThreadExercise", L"练习7 - 跨线程 PostMessage 进度条",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 170,
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
