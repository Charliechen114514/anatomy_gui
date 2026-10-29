/**
 * 21_menu_resource - 菜单资源示例
 *
 * 演示 Win32 菜单资源 (.rc) 的用法：
 * - 从 .rc 资源文件加载菜单 (LoadMenu)
 * - 处理菜单项的 WM_COMMAND 消息
 * - 菜单项的启用/禁用 (EnableMenuItem)
 * - 多行 Edit 控件作为简易文本编辑器
 * - 编辑操作 (撤销、剪切、复制、粘贴) 通过 SendMessage 发送给 Edit 控件
 * - 窗口标题随文档状态变化
 *
 * 参考: tutorial/native_win32/13_ProgrammingGUI_NativeWindows_MenuResource.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include "resource.h"

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"MenuResourceClass";

// Edit 控件 ID
#define ID_EDIT_MAIN  5001

/**
 * 应用程序状态
 *
 * 跟踪文档修改状态和文档名称，
 * 用于控制菜单项的启用/禁用以及窗口标题更新。
 */
struct AppState
{
    HWND hEdit;                 // 多行编辑控件句柄
    bool docModified;           // 文档是否被修改
    wchar_t docName[256];       // 当前文档名称
};

/**
 * 更新窗口标题
 *
 * 格式: "文档名 - 菜单资源示例"
 * 如果文档被修改，在文档名后添加 "*" 标记。
 */
void UpdateWindowTitle(HWND hwnd, AppState* state)
{
    wchar_t title[512] = {};
    if (state->docModified)
    {
        wsprintf(title, L"%s * - 菜单资源示例", state->docName);
    }
    else
    {
        wsprintf(title, L"%s - 菜单资源示例", state->docName);
    }
    SetWindowText(hwnd, title);
}

/**
 * 更新菜单项的启用/禁用状态
 *
 * 保存和另存为菜单项在文档被修改后才可用。
 */
void UpdateMenuState(HWND hwnd, AppState* state)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;

    // 根据 docModified 状态启用或禁用保存/另存为
    UINT flags = state->docModified ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_FILE_SAVE, flags | MF_BYCOMMAND);
    EnableMenuItem(hMenu, IDM_FILE_SAVEAS, flags | MF_BYCOMMAND);
}

/**
 * 窗口过程函数
 *
 * 处理窗口消息，包括：
 * - WM_CREATE: 加载菜单、创建 Edit 控件
 * - WM_SIZE: 调整 Edit 控件大小
 * - WM_COMMAND: 处理菜单和 Edit 控件通知
 * - WM_DESTROY: 清理资源并退出
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建消息
     *
     * 初始化应用程序状态，加载菜单资源，创建多行编辑控件。
     */
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCT*)lParam;

        // 初始化应用状态
        state = new AppState();
        state->docModified = false;
        wcscpy_s(state->docName, L"未命名");
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 从资源加载菜单
        HMENU hMenu = LoadMenu(cs->hInstance, MAKEINTRESOURCE(IDR_MAINMENU));
        if (hMenu)
        {
            SetMenu(hwnd, hMenu);
        }

        // 创建多行编辑控件作为文本编辑区域
        state->hEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,       // 凹陷边框样式
            L"EDIT",                // 编辑控件类名
            L"",                    // 初始文本
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL,
            0, 0, 0, 0,            // 初始大小为 0，在 WM_SIZE 中调整
            hwnd,
            (HMENU)ID_EDIT_MAIN,
            cs->hInstance,
            nullptr
        );

        // 设置编辑控件字体
        HFONT hFont = CreateFont(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        SendMessage(state->hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 初始化窗口标题
        UpdateWindowTitle(hwnd, state);

        return 0;
    }

    /**
     * WM_SIZE - 窗口大小改变消息
     *
     * 当窗口大小改变时，调整编辑控件填满整个客户区。
     */
    case WM_SIZE:
    {
        if (state && state->hEdit)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            MoveWindow(state->hEdit, 0, 0, rect.right, rect.bottom, TRUE);
        }
        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理所有菜单项命令和编辑控件通知。
     */
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // 处理编辑控件内容变更通知
        if (wmId == ID_EDIT_MAIN && wmEvent == EN_CHANGE)
        {
            if (!state->docModified)
            {
                state->docModified = true;
                UpdateWindowTitle(hwnd, state);
                UpdateMenuState(hwnd, state);
            }
            return 0;
        }

        // 处理菜单命令
        switch (wmId)
        {
        /**
         * 新建文档
         * 清空编辑控件，重置文档状态，启用保存/另存为菜单。
         */
        case IDM_FILE_NEW:
        {
            SetWindowText(state->hEdit, L"");
            wcscpy_s(state->docName, L"未命名");
            state->docModified = true;
            UpdateWindowTitle(hwnd, state);
            UpdateMenuState(hwnd, state);
            SetFocus(state->hEdit);
            return 0;
        }

        /**
         * 打开文件
         * 这里仅演示菜单响应，显示功能未实现提示。
         */
        case IDM_FILE_OPEN:
        {
            MessageBox(hwnd,
                L"打开文件功能仅作演示\n实际实现需要使用 GetOpenFileName 对话框",
                L"提示", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        /**
         * 保存文件
         * 这里仅演示菜单响应，显示保存成功的模拟信息。
         */
        case IDM_FILE_SAVE:
        {
            state->docModified = false;
            UpdateWindowTitle(hwnd, state);
            UpdateMenuState(hwnd, state);
            MessageBox(hwnd, L"文件已保存（模拟）", L"保存", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        /**
         * 另存为
         * 这里仅演示菜单响应。
         */
        case IDM_FILE_SAVEAS:
        {
            state->docModified = false;
            UpdateWindowTitle(hwnd, state);
            UpdateMenuState(hwnd, state);
            MessageBox(hwnd, L"文件已另存为（模拟）", L"另存为", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        /**
         * 退出程序
         */
        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;

        /**
         * 撤销 - 发送 EM_UNDO 给编辑控件
         */
        case IDM_EDIT_UNDO:
            if (state && state->hEdit)
            {
                SendMessage(state->hEdit, EM_UNDO, 0, 0);
            }
            return 0;

        /**
         * 剪切 - 发送 WM_CUT 给编辑控件
         */
        case IDM_EDIT_CUT:
            if (state && state->hEdit)
            {
                SendMessage(state->hEdit, WM_CUT, 0, 0);
            }
            return 0;

        /**
         * 复制 - 发送 WM_COPY 给编辑控件
         */
        case IDM_EDIT_COPY:
            if (state && state->hEdit)
            {
                SendMessage(state->hEdit, WM_COPY, 0, 0);
            }
            return 0;

        /**
         * 粘贴 - 发送 WM_PASTE 给编辑控件
         */
        case IDM_EDIT_PASTE:
            if (state && state->hEdit)
            {
                SendMessage(state->hEdit, WM_PASTE, 0, 0);
            }
            return 0;

        /**
         * 关于对话框
         */
        case IDM_HELP_ABOUT:
            MessageBox(hwnd,
                L"菜单资源示例\n\n"
                L"演示 Win32 菜单资源的用法：\n"
                L"- 从 .rc 文件加载菜单\n"
                L"- 菜单项的启用/禁用\n"
                L"- 编辑操作 (撤销、剪切、复制、粘贴)",
                L"关于", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    }

    /**
     * WM_DESTROY - 窗口销毁消息
     *
     * 释放应用程序状态并退出消息循环。
     */
    case WM_DESTROY:
        delete state;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
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
 *
 * 窗口大小: 600x400
 * 菜单在 WM_CREATE 中从资源加载。
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"菜单资源示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        nullptr,
        nullptr,
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
