/**
 * 20_resource_files - 资源文件基础示例
 *
 * 演示 Win32 资源文件 (.rc) 的基本用法：
 * - 从资源加载菜单 (LoadMenu + SetMenu)
 * - 从字符串表加载字符串 (LoadString)
 * - 显示版本信息资源
 * - WM_PAINT 中展示从资源加载的字符串
 *
 * 参考: tutorial/native_win32/12_ProgrammingGUI_NativeWindows_ResourceFiles.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include "resource.h"

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"ResourceFilesClass";

/**
 * 窗口过程函数
 *
 * 处理窗口消息，包括：
 * - WM_CREATE: 加载菜单
 * - WM_PAINT: 显示从字符串表加载的文本
 * - WM_COMMAND: 处理菜单命令（关于、退出）
 * - WM_DESTROY: 退出程序
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建消息
     *
     * 在窗口创建时从资源文件加载菜单并设置到窗口。
     * LoadMenu 从 .rc 文件编译后的资源中加载菜单。
     * SetMenu 将加载的菜单设置为主窗口的菜单栏。
     */
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCT*)lParam;
        // 从资源加载菜单
        HMENU hMenu = LoadMenu(cs->hInstance, MAKEINTRESOURCE(IDR_MYMENU));
        if (hMenu)
        {
            SetMenu(hwnd, hMenu);
        }
        return 0;
    }

    /**
     * WM_PAINT - 绘制消息
     *
     * 在窗口客户区显示从字符串表资源加载的文本内容。
     * 使用 LoadString 从 STRINGTABLE 资源中读取字符串。
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 从字符串表加载字符串
        wchar_t szTitle[256] = {};
        wchar_t szDesc[256] = {};
        wchar_t szVersion[256] = {};

        LoadString(
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            IDS_APP_TITLE,
            szTitle, 256);

        LoadString(
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            IDS_DESCRIPTION,
            szDesc, 256);

        LoadString(
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            IDS_VERSION,
            szVersion, 256);

        // 获取客户区大小
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 设置文本颜色和背景模式
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // 计算垂直间距，居中显示三行文本
        int lineHeight = 40;
        int totalHeight = lineHeight * 3;
        int startY = (rect.bottom - totalHeight) / 2;

        // 绘制标题（加粗）
        RECT lineRect = rect;
        lineRect.top = startY;
        lineRect.bottom = startY + lineHeight;

        HFONT hFontBold = CreateFont(
            -24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        HFONT hFontOld = (HFONT)SelectObject(hdc, hFontBold);
        DrawText(hdc, szTitle, -1, &lineRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 绘制描述文本
        lineRect.top += lineHeight;
        lineRect.bottom += lineHeight;
        HFONT hFontNormal = CreateFont(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        SelectObject(hdc, hFontNormal);
        DrawText(hdc, szDesc, -1, &lineRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 绘制版本信息
        lineRect.top += lineHeight;
        lineRect.bottom += lineHeight;
        DrawText(hdc, szVersion, -1, &lineRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 清理字体
        SelectObject(hdc, hFontOld);
        DeleteObject(hFontBold);
        DeleteObject(hFontNormal);

        EndPaint(hwnd, &ps);
        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理来自菜单的命令：
     * - IDM_ABOUT: 显示关于对话框，展示版本信息
     * - IDM_EXIT: 退出应用程序
     */
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
        {
            // 从字符串表加载版本信息并显示
            wchar_t szVersion[256] = {};
            LoadString(
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                IDS_VERSION,
                szVersion, 256);

            // 构建关于信息
            wchar_t szAbout[512] = {};
            wsprintf(szAbout, L"%s\n\n%s\n%s",
                L"资源文件基础示例",
                L"演示 Win32 资源文件 (.rc) 的基本用法",
                szVersion);

            MessageBox(hwnd, szAbout, L"关于", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case IDM_EXIT:
            // 发送关闭消息，触发 WM_DESTROY
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    /**
     * WM_DESTROY - 窗口销毁消息
     */
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/**
 * 注册窗口类
 */
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

/**
 * 创建主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"资源文件基础示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 400,
        nullptr,
        nullptr,      // 菜单在 WM_CREATE 中从资源加载
        hInstance,
        nullptr
    );

    if (hwnd == nullptr)
    {
        return nullptr;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

/**
 * wWinMain - Windows 应用程序入口点 (Unicode 版本)
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 步骤 1: 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 步骤 2: 创建主窗口
    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 步骤 3: 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
