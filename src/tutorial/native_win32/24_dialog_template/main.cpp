/**
 * 对话框模板资源示例 (Example 24)
 *
 * 本示例演示如何使用 .rc 资源文件中定义的 DIALOG 模板来创建模态对话框。
 * 对话框的布局、控件、样式全部在 resources.rc 中声明，
 * 代码中仅需调用 DialogBox + MAKEINTRESOURCE 即可弹出。
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include "resource.h"

// ============================================================================
// 全局变量
// ============================================================================
static HINSTANCE g_hInst   = nullptr;   // 应用程序实例句柄
static HWND      g_hMainWnd = nullptr;  // 主窗口句柄
static HWND      g_hStatic  = nullptr;  // 用于显示对话框结果的静态文本

// ============================================================================
// 对话框过程 —— 处理"关于"对话框的消息
// ============================================================================
INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // ---- 对话框初始化 ----
        // 加载应用程序图标并设置到对话框标题栏
        HICON hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hDlg, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);

        // 动态更新版本信息文本（演示可在代码中修改模板控件内容）
        SetDlgItemText(hDlg, IDC_APP_VERSION, L"版本 1.0.0 (动态设置)");

        return TRUE;   // 已处理，焦点设为默认控件
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            // 用户点击"确定"，返回 IDOK
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            // 用户点击"取消"，返回 IDCANCEL
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        // 点击关闭按钮也视为取消
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;   // 未处理的消息交给默认对话框过程
}

// ============================================================================
// 主窗口过程
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // ---- 创建"打开关于对话框"按钮 ----
        CreateWindowEx(
            0,
            L"BUTTON",
            L"打开关于对话框",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 100, 210, 40,
            hWnd,
            (HMENU)1001,       // 按钮控件 ID
            g_hInst,
            nullptr
        );

        // ---- 创建静态文本，用于显示对话框返回结果 ----
        g_hStatic = CreateWindowEx(
            0,
            L"STATIC",
            L"对话框结果: (尚未打开)",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            50, 170, 350, 30,
            hWnd,
            (HMENU)1002,
            g_hInst,
            nullptr
        );

        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case 1001:
        {
            // ---- 按钮被点击，打开模态对话框 ----
            // DialogBox 从 .rc 资源中加载对话框模板
            // MAKEINTRESOURCE 将整数资源 ID 转换为资源名称
            INT_PTR nResult = DialogBox(
                g_hInst,                        // 实例句柄（包含 .rc 资源）
                MAKEINTRESOURCE(IDD_ABOUT_DIALOG),  // 对话框模板资源 ID
                hWnd,                           // 父窗口
                AboutDialogProc                 // 对话框过程
            );

            // 根据对话框返回值更新静态文本
            switch (nResult)
            {
            case IDOK:
                SetWindowText(g_hStatic, L"对话框结果: 用户点击了「确定」(IDOK)");
                break;
            case IDCANCEL:
                SetWindowText(g_hStatic, L"对话框结果: 用户点击了「取消」(IDCANCEL)");
                break;
            default:
                SetWindowText(g_hStatic, L"对话框结果: 未知返回值");
                break;
            }

            // 强制重绘静态文本
            InvalidateRect(g_hStatic, nullptr, TRUE);
            return 0;
        }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// WinMain —— 程序入口
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    g_hInst = hInstance;

    // ---- 注册主窗口类 ----
    WNDCLASSEX wc   = { sizeof(WNDCLASSEX) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DialogTemplateDemoClass";
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassEx(&wc);

    // ---- 创建主窗口 ----
    g_hMainWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"对话框模板资源示例",          // 窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        450, 300,                         // 窗口尺寸 450×300
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    // ---- 消息循环 ----
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        // 对于非模态对话框可加入 IsDialogMessage，
        // 此处仅使用模态对话框，标准循环即可
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
