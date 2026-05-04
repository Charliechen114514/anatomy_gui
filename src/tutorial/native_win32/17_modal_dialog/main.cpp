/**
 * 17_modal_dialog - Win32 模态对话框示例程序
 *
 * 本程序演示 Win32 模态对话框的创建和使用：
 * 1. 在内存中构建对话框模板 (DLGTEMPLATE + DLGITEMTEMPLATE) - 无需资源文件
 * 2. 使用 DialogBoxIndirectParam 创建模态对话框 - 阻塞父窗口
 * 3. 对话框过程 (DialogProc) 处理 WM_INITDIALOG 和 WM_COMMAND
 * 4. EndDialog 关闭对话框并返回结果值 (IDOK / IDCANCEL)
 * 5. 主窗口根据对话框返回值更新显示内容
 *
 * 编译方式:
 *   cmake -B build
 *   cmake --build build
 *
 * 参考: tutorial/native_win32/9_ProgrammingGUI_NativeWindows_ModalDialog.md
 */

#ifndef UNICODE
#define UNICODE  // 使用 Unicode 字符集
#endif

#include <windows.h>
#include <cstdio>

// =============================================================================
// 常量定义
// =============================================================================

// 窗口类名称 - 必须是唯一的
static const wchar_t* WINDOW_CLASS_NAME = L"ModalDialogDemoClass";

// 控件 ID
static const int IDB_OPEN_ABOUT    = 101;   // "打开关于对话框" 按钮
static const int IDS_RESULT_LABEL  = 102;   // 显示结果的静态文本

// 对话框中控件的 ID
static const int IDS_APP_NAME     = 201;   // 应用名称标签
static const int IDS_APP_VERSION  = 202;   // 版本号标签
static const int IDS_DESCRIPTION  = 203;   // 描述信息标签

// 全局变量 - 存储对话框返回的结果
static wchar_t g_resultText[256] = L"尚未打开对话框";
static INT_PTR g_dialogResult = 0;

// 全局实例句柄 - DialogBoxIndirectParam 需要
static HINSTANCE g_hInstance = nullptr;

// =============================================================================
// 辅助函数 - 在内存中构建对话框模板
// =============================================================================

/**
 * 在内存中创建 "关于" 对话框模板
 *
 * 对话框模板由 DLGTEMPLATE 头部和紧随其后的可变长度数据组成，
 * 后面跟着每个控件的 DLGITEMTEMPLATE 及其可变长度数据。
 *
 * 所有字符串必须以 WORD 对齐方式存储（不足则补 '\0'）。
 * 对话框中的坐标使用"对话框单位"（不是像素）。
 *
 * 返回: 分配的内存指针，调用者负责释放 (GlobalFree)
 */
LPDLGTEMPLATE CreateAboutDialogTemplate()
{
    // ---------------------------------------------------------------
    // 计算所需内存大小
    // ---------------------------------------------------------------
    //
    // 对话框模板结构:
    //   DLGTEMPLATE (固定部分)
    //   + menu 字符串 (空字符串, 即一个 '\0')
    //   + windowClass 字符串 (空字符串, 即一个 '\0')
    //   + title 字符串 (L"关于")
    //   + 3 个 DLGITEMTEMPLATE (应用名称、版本、按钮 x2)
    //
    // 每个控件项:
    //   DLGITEMTEMPLATE (固定部分)
    //   + class 字符串或原子
    //   + title 字符串
    //   + 创建数据 (为空)

    // 我们有 5 个控件: 应用名称、版本、描述、确定按钮、取消按钮
    const int NUM_CONTROLS = 5;

    // 估算足够大的缓冲区
    const size_t bufSize = 1024;
    LPDLGTEMPLATE lpDlg = (LPDLGTEMPLATE)GlobalAlloc(GPTR, bufSize);
    if (lpDlg == nullptr)
    {
        return nullptr;
    }

    // 使用指针遍历缓冲区，逐步填充模板
    BYTE* ptr = (BYTE*)lpDlg;

    // ---------------------------------------------------------------
    // 1. 填充 DLGTEMPLATE 结构体
    // ---------------------------------------------------------------
    lpDlg->style = WS_POPUP | WS_CAPTION | WS_SYSMENU
                 | DS_MODALFRAME | DS_CENTER;
    lpDlg->dwExtendedStyle = 0;
    lpDlg->cdit = NUM_CONTROLS;   // 控件数量
    lpDlg->x = 0;
    lpDlg->y = 0;
    lpDlg->cx = 220;              // 对话框宽度 (对话框单位)
    lpDlg->cy = 120;              // 对话框高度 (对话框单位)

    ptr += sizeof(DLGTEMPLATE);

    // ---------------------------------------------------------------
    // 2. 菜单名 - 空字符串 (无菜单)
    // ---------------------------------------------------------------
    // DLGTEMPLATE 后面紧跟着三个以 '\0' 结尾的 Unicode 字符串:
    //   menu, windowClass, title
    // 空字符串只需要一个 '\0' 字符 (wchar_t)
    *((wchar_t*)ptr) = L'\0';
    ptr += sizeof(wchar_t);

    // ---------------------------------------------------------------
    // 3. 窗口类名 - 空字符串 (使用默认对话框类)
    // ---------------------------------------------------------------
    *((wchar_t*)ptr) = L'\0';
    ptr += sizeof(wchar_t);

    // ---------------------------------------------------------------
    // 4. 对话框标题
    // ---------------------------------------------------------------
    const wchar_t* title = L"关于";
    size_t titleLen = wcslen(title) + 1;   // 包含结尾 '\0'
    memcpy(ptr, title, titleLen * sizeof(wchar_t));
    ptr += titleLen * sizeof(wchar_t);

    // 确保 ptr 按 WORD (2 字节) 对齐
    // DLGITEMTEMPLATE 要求 DWORD (4 字节) 对齐
    while ((ULONG_PTR)ptr % sizeof(DWORD) != 0)
    {
        ptr++;
    }

    // ---------------------------------------------------------------
    // 5. 添加控件: 应用名称标签 "模态对话框示例"
    // ---------------------------------------------------------------
    // DLGITEMTEMPLATE + class + title + creationData

    {
        LPDLGITEMTEMPLATE lpItem = (LPDLGITEMTEMPLATE)ptr;
        lpItem->style = WS_CHILD | WS_VISIBLE | SS_CENTER;
        lpItem->dwExtendedStyle = 0;
        lpItem->x = 10;
        lpItem->y = 10;
        lpItem->cx = 200;
        lpItem->cy = 15;
        lpItem->id = IDS_APP_NAME;

        ptr += sizeof(DLGITEMTEMPLATE);

        // class: 使用预定义控件的原子值
        // 0x0082 是 STATIC 控件的原子值
        // 参考: MSDN "Static Control" 的控件类原子
        *((WORD*)ptr) = 0xFFFF;   // 预定义控件标记
        ptr += sizeof(WORD);
        *((WORD*)ptr) = 0x0082;   // STATIC 控件原子
        ptr += sizeof(WORD);

        // title: 应用名称
        const wchar_t* szAppName = L"模态对话框示例";
        size_t len = wcslen(szAppName) + 1;
        memcpy(ptr, szAppName, len * sizeof(wchar_t));
        ptr += len * sizeof(wchar_t);

        // 创建数据: 无额外数据，写 0 表示长度为 0
        *((WORD*)ptr) = 0;
        ptr += sizeof(WORD);

        // 对齐到 DWORD 边界
        while ((ULONG_PTR)ptr % sizeof(DWORD) != 0)
        {
            ptr++;
        }
    }

    // ---------------------------------------------------------------
    // 6. 添加控件: 版本号标签 "v1.0"
    // ---------------------------------------------------------------
    {
        LPDLGITEMTEMPLATE lpItem = (LPDLGITEMTEMPLATE)ptr;
        lpItem->style = WS_CHILD | WS_VISIBLE | SS_CENTER;
        lpItem->dwExtendedStyle = 0;
        lpItem->x = 10;
        lpItem->y = 30;
        lpItem->cx = 200;
        lpItem->cy = 15;
        lpItem->id = IDS_APP_VERSION;

        ptr += sizeof(DLGITEMTEMPLATE);

        *((WORD*)ptr) = 0xFFFF;   // 预定义控件标记
        ptr += sizeof(WORD);
        *((WORD*)ptr) = 0x0082;   // STATIC 控件原子
        ptr += sizeof(WORD);

        const wchar_t* szVersion = L"v1.0";
        size_t len = wcslen(szVersion) + 1;
        memcpy(ptr, szVersion, len * sizeof(wchar_t));
        ptr += len * sizeof(wchar_t);

        *((WORD*)ptr) = 0;
        ptr += sizeof(WORD);

        while ((ULONG_PTR)ptr % sizeof(DWORD) != 0)
        {
            ptr++;
        }
    }

    // ---------------------------------------------------------------
    // 7. 添加控件: 描述信息标签
    // ---------------------------------------------------------------
    {
        LPDLGITEMTEMPLATE lpItem = (LPDLGITEMTEMPLATE)ptr;
        lpItem->style = WS_CHILD | WS_VISIBLE | SS_CENTER;
        lpItem->dwExtendedStyle = 0;
        lpItem->x = 10;
        lpItem->y = 50;
        lpItem->cx = 200;
        lpItem->cy = 25;
        lpItem->id = IDS_DESCRIPTION;

        ptr += sizeof(DLGITEMTEMPLATE);

        *((WORD*)ptr) = 0xFFFF;   // 预定义控件标记
        ptr += sizeof(WORD);
        *((WORD*)ptr) = 0x0082;   // STATIC 控件原子
        ptr += sizeof(WORD);

        const wchar_t* szDesc = L"这是一个 Win32 模态对话框示例\r\n程序化创建对话框模板";
        size_t len = wcslen(szDesc) + 1;
        memcpy(ptr, szDesc, len * sizeof(wchar_t));
        ptr += len * sizeof(wchar_t);

        *((WORD*)ptr) = 0;
        ptr += sizeof(WORD);

        while ((ULONG_PTR)ptr % sizeof(DWORD) != 0)
        {
            ptr++;
        }
    }

    // ---------------------------------------------------------------
    // 8. 添加控件: "确定" 按钮 (IDOK)
    // ---------------------------------------------------------------
    {
        LPDLGITEMTEMPLATE lpItem = (LPDLGITEMTEMPLATE)ptr;
        lpItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
        lpItem->dwExtendedStyle = 0;
        lpItem->x = 30;
        lpItem->y = 90;
        lpItem->cx = 70;
        lpItem->cy = 20;
        lpItem->id = IDOK;

        ptr += sizeof(DLGITEMTEMPLATE);

        *((WORD*)ptr) = 0xFFFF;   // 预定义控件标记
        ptr += sizeof(WORD);
        *((WORD*)ptr) = 0x0080;   // BUTTON 控件原子
        ptr += sizeof(WORD);

        const wchar_t* szOK = L"确定";
        size_t len = wcslen(szOK) + 1;
        memcpy(ptr, szOK, len * sizeof(wchar_t));
        ptr += len * sizeof(wchar_t);

        *((WORD*)ptr) = 0;
        ptr += sizeof(WORD);

        while ((ULONG_PTR)ptr % sizeof(DWORD) != 0)
        {
            ptr++;
        }
    }

    // ---------------------------------------------------------------
    // 9. 添加控件: "取消" 按钮 (IDCANCEL)
    // ---------------------------------------------------------------
    {
        LPDLGITEMTEMPLATE lpItem = (LPDLGITEMTEMPLATE)ptr;
        lpItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
        lpItem->dwExtendedStyle = 0;
        lpItem->x = 120;
        lpItem->y = 90;
        lpItem->cx = 70;
        lpItem->cy = 20;
        lpItem->id = IDCANCEL;

        ptr += sizeof(DLGITEMTEMPLATE);

        *((WORD*)ptr) = 0xFFFF;   // 预定义控件标记
        ptr += sizeof(WORD);
        *((WORD*)ptr) = 0x0080;   // BUTTON 控件原子
        ptr += sizeof(WORD);

        const wchar_t* szCancel = L"取消";
        size_t len = wcslen(szCancel) + 1;
        memcpy(ptr, szCancel, len * sizeof(wchar_t));
        ptr += len * sizeof(wchar_t);

        *((WORD*)ptr) = 0;
        ptr += sizeof(WORD);

        // 最后一个控件不需要再对齐
    }

    return lpDlg;
}

// =============================================================================
// 对话框过程函数
// =============================================================================

/**
 * 对话框过程 (Dialog Procedure)
 *
 * 与窗口过程 (WindowProc) 类似，但有以下区别:
 * 1. 使用 INT_PTR 返回类型 (不是 LRESULT)
 * 2. 返回 TRUE 表示已处理消息，FALSE 表示未处理
 * 3. 不调用 DefWindowProc - 对话框管理器会处理默认行为
 * 4. 接收 WM_INITDIALOG 而不是 WM_CREATE
 * 5. 使用 EndDialog 关闭对话框 (不是 DestroyWindow)
 *
 * 参数说明:
 *   hwndDlg - 对话框窗口句柄
 *   uMsg    - 消息标识符
 *   wParam  - 消息附加参数
 *   lParam  - 消息附加参数 (DialogBoxIndirectParam 传入的最后一个参数)
 */
INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_INITDIALOG - 对话框初始化消息
     *
     * 在对话框显示之前发送，相当于窗口的 WM_CREATE。
     * 可以在这里初始化控件状态、设置字体等。
     *
     * 参数:
     *   wParam - 将要接收默认键盘焦点的控件句柄
     *   lParam - DialogBoxIndirectParam 传入的 dwInitParam
     *
     * 返回 TRUE 将键盘焦点设置到 wParam 指定的控件。
     * 返回 FALSE 则不设置默认焦点 (如果我们自己设置了焦点)。
     */
    case WM_INITDIALOG:
    {
        // 设置应用名称标签的字体为粗体
        // 获取控件的字体
        HWND hAppName = GetDlgItem(hwndDlg, IDS_APP_NAME);
        HFONT hFont = CreateFont(
            16,                        // 高度
            0,                         // 宽度 (0 = 自动)
            0, 0,                      // 文本倾斜角和基线倾斜角
            FW_BOLD,                   // 粗细: 粗体
            FALSE, FALSE, FALSE,       // 斜体、下划线、删除线
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei"         // 字体名称
        );
        SendMessage(hAppName, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 将字体也设置给版本号标签和描述标签
        SendMessage(GetDlgItem(hwndDlg, IDS_APP_VERSION), WM_SETFONT, (WPARAM)hFont, TRUE);

        // 返回 TRUE 让系统设置默认焦点 (第一个有 WS_TABSTOP 样式的控件)
        return TRUE;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 当用户点击按钮、选择菜单项等控件时发送。
     *
     * 参数:
     *   wParam - 低字位 (LOWORD) 包含控件 ID，高字位 (HIWORD) 包含通知码
     *   lParam - 发送消息的控件句柄
     */
    case WM_COMMAND:
    {
        WORD ctrlId = LOWORD(wParam);
        WORD notifyCode = HIWORD(wParam);

        // BN_CLICKED (通知码 0) 表示按钮被点击
        if (notifyCode == BN_CLICKED)
        {
            switch (ctrlId)
            {
            case IDOK:
                g_dialogResult = IDOK;
                EndDialog(hwndDlg, IDOK);
                return TRUE;

            case IDCANCEL:
                g_dialogResult = IDCANCEL;
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
        }
        break;
    }

    /**
     * WM_CLOSE - 窗口关闭消息
     *
     * 当用户点击系统菜单的 "关闭" 或按下 Alt+F4 时发送。
     * 对于模态对话框，应该使用 EndDialog 关闭。
     */
    case WM_CLOSE:
        g_dialogResult = IDCANCEL;
        EndDialog(hwndDlg, IDCANCEL);
        return TRUE;
    }

    // 对于我们未处理的消息，返回 FALSE
    // 对话框管理器会进行默认处理
    return FALSE;
}

// =============================================================================
// 窗口过程函数
// =============================================================================

/**
 * 窗口过程函数 (Window Procedure)
 *
 * 处理主窗口的所有消息。
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建消息
     *
     * 在 CreateWindowEx 返回之前发送，用于初始化窗口和创建子控件。
     * lParam 指向 CREATESTRUCT 结构体。
     */
    case WM_CREATE:
    {
        // 创建 "打开关于对话框" 按钮
        CreateWindowEx(
            0,                                      // 扩展样式
            L"BUTTON",                              // 控件类名
            L"打开关于对话框",                       // 按钮文本
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,  // 样式
            140, 80,                                // 位置 (x, y)
            220, 40,                                // 大小 (宽, 高)
            hwnd,                                   // 父窗口
            (HMENU)IDB_OPEN_ABOUT,                  // 控件 ID
            g_hInstance,                            // 实例句柄
            nullptr                                 // 附加数据
        );

        // 创建显示结果的静态文本
        CreateWindowEx(
            0,                                      // 扩展样式
            L"STATIC",                              // 控件类名
            g_resultText,                           // 初始文本
            WS_CHILD | WS_VISIBLE | SS_CENTER,      // 样式
            50, 160,                                // 位置 (x, y)
            400, 30,                                // 大小 (宽, 高)
            hwnd,                                   // 父窗口
            (HMENU)IDS_RESULT_LABEL,                // 控件 ID
            g_hInstance,                            // 实例句柄
            nullptr                                 // 附加数据
        );

        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理按钮点击等控件通知。
     */
    case WM_COMMAND:
    {
        WORD ctrlId = LOWORD(wParam);
        WORD notifyCode = HIWORD(wParam);

        if (notifyCode == BN_CLICKED && ctrlId == IDB_OPEN_ABOUT)
        {
            // 在内存中创建对话框模板
            LPDLGTEMPLATE lpDlgTemplate = CreateAboutDialogTemplate();
            if (lpDlgTemplate == nullptr)
            {
                MessageBox(hwnd, L"创建对话框模板失败!", L"错误", MB_ICONERROR);
                return 0;
            }

            /**
             * DialogBoxIndirectParam - 创建并显示模态对话框
             *
             * 参数:
             *   hInstance      - 应用程序实例句柄
             *   lpTemplate     - 指向 DLGTEMPLATE 的指针 (内存中的对话框模板)
             *   hWndParent     - 父窗口句柄 (模态对话框会阻塞此窗口)
             *   lpDialogFunc   - 对话框过程函数指针
             *   dwInitParam    - 传递给 WM_INITDIALOG 的 lParam 参数
             *
             * 返回值:
             *   对话框调用 EndDialog 时传入的返回值。
             *   如果对话框创建失败，返回 -1。
             *
             * 重要特性:
             * - 模态对话框创建后，父窗口被禁用 (无法响应输入)
             * - DialogBoxIndirectParam 不会返回，直到对话框关闭
             * - 内部有自己的消息循环，但属于同一线程
             */
            g_dialogResult = 0;
            INT_PTR result = DialogBoxIndirectParam(
                g_hInstance,              // 实例句柄
                lpDlgTemplate,            // 对话框模板
                hwnd,                     // 父窗口 (会被阻塞)
                AboutDialogProc,          // 对话框过程
                0                         // 传递给 WM_INITDIALOG 的参数
            );

            // 释放对话框模板内存
            GlobalFree((HGLOBAL)lpDlgTemplate);

            if (g_dialogResult == IDOK || result == IDOK)
            {
                wcscpy_s(g_resultText, L"用户点击了「确定」(IDOK)");
            }
            else if (g_dialogResult == IDCANCEL || result == IDCANCEL)
            {
                wcscpy_s(g_resultText, L"用户点击了「取消」(IDCANCEL)");
            }
            else
            {
                wcscpy_s(g_resultText, L"对话框返回了未知结果");
            }

            // 更新静态文本控件的显示内容
            SetDlgItemText(hwnd, IDS_RESULT_LABEL, g_resultText);

            return 0;
        }
        break;
    }

    /**
     * WM_CTLCOLORSTATIC - 静态控件颜色消息
     *
     * 当静态控件需要绘制时发送给父窗口，用于设置文本颜色和背景。
     */
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        // 为结果标签设置蓝色文字
        if (GetDlgCtrlID(hCtrl) == IDS_RESULT_LABEL)
        {
            SetTextColor(hdcStatic, RGB(0, 80, 180));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    }

    /**
     * WM_PAINT - 绘制消息
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区矩形
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 绘制标题说明文本
        SetTextColor(hdc, RGB(60, 60, 60));
        SetBkMode(hdc, TRANSPARENT);

        // 在按钮上方绘制说明
        RECT titleRect = { 0, 30, rect.right, 70 };
        DrawText(
            hdc,
            L"点击下方按钮打开模态对话框",
            -1,
            &titleRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        EndPaint(hwnd, &ps);
        return 0;
    }

    /**
     * WM_DESTROY - 窗口销毁消息
     */
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        // 对于我们不处理的消息，交给系统的默认处理函数
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

// =============================================================================
// 注册窗口类
// =============================================================================

/**
 * 注册窗口类
 *
 * 在创建窗口之前，必须先注册窗口类。
 * 窗口类定义了窗口的属性和行为。
 *
 * 参数:
 *   hInstance - 应用程序实例句柄
 *
 * 返回:
 *   成功返回非零值，失败返回 0
 */
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    return RegisterClassEx(&wc);
}

// =============================================================================
// 主函数
// =============================================================================

/**
 * wWinMain - Windows 应用程序入口点 (Unicode 版本)
 *
 * 这是 Windows GUI 程序的入口函数，相当于控制台程序的 main 函数。
 *
 * 参数说明:
 *   hInstance     - 当前应用程序的实例句柄
 *   hPrevInstance - 前一个实例的句柄 (Win32 中始终为 nullptr)
 *   pCmdLine      - 命令行参数字符串 (Unicode 版本)
 *   nCmdShow      - 窗口初始显示状态
 *
 * 返回值:
 *   程序退出码，通常来自 WM_QUIT 消息的 wParam
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    // 未使用的参数标记 (避免编译器警告)
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 保存实例句柄到全局变量
    g_hInstance = hInstance;

    // 步骤 1: 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 步骤 2: 创建主窗口
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"模态对话框示例",               // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,   // 窗口位置 (x, y)
        500, 300,                       // 窗口大小 (宽度, 高度)
        nullptr,                        // 父窗口句柄
        nullptr,                        // 菜单句柄
        hInstance,                      // 应用程序实例句柄
        nullptr                         // 创建参数
    );

    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);

    // 更新窗口 (触发 WM_PAINT 消息)
    UpdateWindow(hwnd);

    // 步骤 3: 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 步骤 4: 退出程序
    return (int)msg.wParam;
}
