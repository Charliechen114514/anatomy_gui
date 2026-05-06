#include <windows.h>
#include <commctrl.h>
#include <cstdio>

#define IDC_EDIT       1001
#define IDC_SUBMIT_BTN 1002
#define IDC_RESULT     1003

// 自定义消息：Enter 键被按下
#define WM_ENTER_PRESSED (WM_APP + 10)

HWND g_hEdit = NULL;
HWND g_hResult = NULL;

// 子类过程
LRESULT CALLBACK EditSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            // 拦截 Enter 键，通知父窗口
            HWND hParent = GetParent(hWnd);
            SendMessage(hParent, WM_ENTER_PRESSED, 0, 0);
            return 0;  // 不传给原始 Edit 过程
        }
        break;

    case WM_CHAR:
        // 拦截 Enter 的字符（换行符），防止多行 Edit 插入换行
        if (wParam == '\r' || wParam == '\n')
            return 0;
        break;

    case WM_DESTROY:
        // 窗口销毁时移除子类化
        RemoveWindowSubclass(hWnd, EditSubclassProc, 0);
        break;
    }

    // 其他消息交给原始窗口过程处理
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void SubmitText(HWND hwnd)
{
    wchar_t buf[256];
    GetWindowText(g_hEdit, buf, 256);

    if (wcslen(buf) == 0)
    {
        MessageBox(hwnd, L"请输入一些文字", L"提示", MB_OK);
        return;
    }

    // 在结果区域显示
    wchar_t result[512];
    swprintf(result, 512, L"已提交：%s", buf);
    SetWindowText(g_hResult, result);

    // 清空输入框
    SetWindowText(g_hEdit, L"");
    SetFocus(g_hEdit);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 创建标签
        CreateWindowEx(0, L"STATIC", L"输入文字，按 Enter 提交：",
            WS_CHILD | WS_VISIBLE,
            20, 20, 300, 20,
            hwnd, NULL, hInst, NULL);

        // 创建 Edit 控件
        g_hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 50, 350, 28,
            hwnd, (HMENU)IDC_EDIT, hInst, NULL);

        // 子类化 Edit 控件
        SetWindowSubclass(g_hEdit, EditSubclassProc, 0, 0);

        // 创建提交按钮
        CreateWindowEx(0, L"BUTTON", L"提交",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 50, 80, 28,
            hwnd, (HMENU)IDC_SUBMIT_BTN, hInst, NULL);

        // 创建结果显示
        g_hResult = CreateWindowEx(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 100, 440, 30,
            hwnd, (HMENU)IDC_RESULT, hInst, NULL);

        SetFocus(g_hEdit);
        return 0;
    }

    case WM_ENTER_PRESSED:
        SubmitText(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SUBMIT_BTN)
            SubmitText(hwnd);
        return 0;

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
    wc.lpszClassName = L"SubclassDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"SubclassDemo", L"子类化示例 - Enter 提交",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 200,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
