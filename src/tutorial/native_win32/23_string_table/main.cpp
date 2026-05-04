// 23_string_table/main.cpp
// 字符串表资源示例
// 演示：STRINGTABLE 资源定义 + LoadString 加载所有 UI 文本
// 参考：tutorial/native_win32/15_ProgrammingGUI_NativeWindows_StringTable.md

#include <windows.h>
#include <windowsx.h>
#include "resource.h"

// ==================== 控件 ID ====================
#define IDC_BTN_CLICK      1001
#define IDC_STATIC_GREET   1002
#define IDC_STATIC_DESC    1003

// ==================== 全局变量 ====================
static HINSTANCE g_hInst      = nullptr;
static int       g_clickCount = 0;

// ==================== 辅助函数：加载字符串资源 ====================
// 封装 LoadStringW，简化调用
static void LoadStringResource(HINSTANCE hInst, UINT id, wchar_t* buf, int bufSize)
{
    if (LoadStringW(hInst, id, buf, bufSize) == 0)
    {
        // 加载失败时提供默认后备文本
        wsprintfW(buf, L"[字符串 ID=%u 加载失败]", id);
    }
}

// ==================== 窗口过程 ====================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        wchar_t szText[256] = {};

        // ---- 静态文字：问候语 (IDS_GREETING) ----
        LoadStringResource(g_hInst, IDS_GREETING, szText, 256);
        CreateWindowExW(0, L"STATIC", szText,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 15, 400, 22,
            hwnd, (HMENU)IDC_STATIC_GREET, g_hInst, nullptr);

        // ---- 静态文字：描述 (IDS_DESCRIPTION) ----
        LoadStringResource(g_hInst, IDS_DESCRIPTION, szText, 256);
        HWND hDesc = CreateWindowExW(0, L"STATIC", szText,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 45, 400, 55,
            hwnd, (HMENU)IDC_STATIC_DESC, g_hInst, nullptr);
        // 多行文字需要设置字体
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"微软雅黑");
        SendMessageW(hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

        // ---- 按钮 (IDS_BTN_CLICK) ----
        LoadStringResource(g_hInst, IDS_BTN_CLICK, szText, 256);
        CreateWindowExW(0, L"BUTTON", szText,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 115, 120, 35,
            hwnd, (HMENU)IDC_BTN_CLICK, g_hInst, nullptr);

        // ---- 说明文字 ----
        CreateWindowExW(0, L"STATIC",
            L"以上所有文字均通过 LoadString 从 STRINGTABLE 加载。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 170, 400, 20,
            hwnd, nullptr, g_hInst, nullptr);

        CreateWindowExW(0, L"STATIC",
            L"关闭窗口时会显示来自字符串表的确认对话框。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 195, 400, 20,
            hwnd, nullptr, g_hInst, nullptr);

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BTN_CLICK:
        {
            g_clickCount++;
            // 从字符串表加载消息模板，用 wsprintfW 格式化
            wchar_t szTemplate[256] = {};
            wchar_t szMessage[256]  = {};
            LoadStringResource(g_hInst, IDS_CLICK_MSG, szTemplate, 256);
            wsprintfW(szMessage, szTemplate, g_clickCount);
            MessageBoxW(hwnd, szMessage, L"提示", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        }
        break;
    }

    case WM_CLOSE:
    {
        // 从字符串表加载确认文本
        wchar_t szConfirm[256] = {};
        LoadStringResource(g_hInst, IDS_EXIT_CONFIRM, szConfirm, 256);

        int result = MessageBoxW(hwnd, szConfirm, L"确认",
                                 MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES)
        {
            DestroyWindow(hwnd);
        }
        return 0;  // 不调用 DefWindowProc，由我们决定是否关闭
    }

    case WM_DESTROY:
    {
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

    // ---- 从字符串表加载窗口标题 ----
    wchar_t szTitle[256] = {};
    LoadStringResource(hInstance, IDS_APP_TITLE, szTitle, 256);

    // ---- 注册窗口类 ----
    const wchar_t CLASS_NAME[] = L"StringTableDemo";

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);

    RegisterClassExW(&wc);

    // ---- 创建主窗口 ----
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME,
        szTitle,                      // 标题来自字符串表
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        450, 300,                     // 窗口大小 450×300
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
