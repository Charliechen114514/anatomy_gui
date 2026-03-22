/**
 * Win32 Window Procedure 示例程序
 *
 * 本程序演示如何编写结构化的窗口过程函数。
 * WindowProc 是 Windows 消息处理的核心，所有发送到窗口的消息
 * 都会通过这个函数进行处理。
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <sstream>

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"WindowProcedureDemo";

// 函数前置声明
LRESULT OnCreate(HWND hwnd [[maybe_unused]], CREATESTRUCT* createStruct [[maybe_unused]]);
LRESULT OnSize(HWND hwnd [[maybe_unused]], UINT flags [[maybe_unused]], int width [[maybe_unused]], int height [[maybe_unused]]);
LRESULT OnPaint(HWND hwnd);
LRESULT OnLButtonDown(HWND hwnd, UINT flags [[maybe_unused]], int x, int y);
LRESULT OnKeyDown(HWND hwnd, UINT vkCode);
LRESULT OnDestroy(HWND hwnd [[maybe_unused]]);

/**
 * 窗口过程函数
 *
 * 这是 Windows 程序的消息处理中心。每当窗口接收到消息时，
 * 系统就会调用这个函数。
 *
 * 参数说明:
 *   hwnd    - 窗口句柄，标识接收消息的窗口
 *   uMsg    - 消息标识符，表示消息的类型
 *   wParam  - 消息的附加参数，含义取决于消息类型
 *   lParam  - 消息的附加参数，含义取决于消息类型
 *
 * 返回值:
 *   处理结果取决于消息类型，未处理的消息应返回 DefWindowProc 的结果
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 使用 switch-case 结构分发消息
    switch (uMsg)
    {
        case WM_CREATE:
            return OnCreate(hwnd, reinterpret_cast<CREATESTRUCT*>(lParam));

        case WM_SIZE:
            return OnSize(hwnd, (UINT)wParam, LOWORD(lParam), HIWORD(lParam));

        case WM_PAINT:
            return OnPaint(hwnd);

        case WM_LBUTTONDOWN:
            return OnLButtonDown(hwnd, (UINT)wParam,
                                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

        case WM_KEYDOWN:
            return OnKeyDown(hwnd, (UINT)wParam);

        case WM_DESTROY:
            return OnDestroy(hwnd);

        default:
            // 默认处理：让系统处理我们未处理的消息
            // 这一点非常重要，确保窗口能正常工作
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/**
 * 处理 WM_CREATE 消息
 *
 * 当窗口通过 CreateWindowEx 创建时，此消息被发送。
 * 这是进行一次性初始化的好机会。
 */
LRESULT OnCreate(HWND hwnd, CREATESTRUCT* createStruct)
{
    // 在这里进行初始化工作
    // 例如：创建子窗口、分配内存、加载资源等

    // 返回 0 表示成功创建窗口
    // 返回 -1 会阻止窗口创建
    return 0;
}

/**
 * 处理 WM_SIZE 消息
 *
 * 当窗口大小改变时发送此消息。
 *
 * 参数说明:
 *   hwnd  - 窗口句柄
 *   flags - 重置大小标志 (SIZE_MAXIMIZED, SIZE_MINIMIZED, SIZE_RESTORED 等)
 *   width - 客户区的新宽度
 *   height - 客户区的新高度
 */
LRESULT OnSize(HWND hwnd, UINT flags, int width, int height)
{
    // 窗口大小变化时的处理
    // 例如：重新排列子窗口、调整布局等

    // 强制重绘客户区
    InvalidateRect(hwnd, nullptr, TRUE);

    return 0;
}

/**
 * 处理 WM_PAINT 消息
 *
 * 当窗口客户区需要重绘时发送此消息。
 * 这是进行所有绘图操作的入口。
 */
LRESULT OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 获取客户区尺寸
    RECT rect;
    GetClientRect(hwnd, &rect);

    // 设置文本颜色和背景模式
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    // 绘制说明文本
    const wchar_t* lines[] = {
        L"Win32 Window Procedure 演示",
        L"",
        L"操作说明:",
        L"  - 点击左键: 显示点击坐标",
        L"  - 按下任意键: 显示虚拟键码",
        L"  - 改变窗口大小: 触发 WM_SIZE",
        L"  - 关闭窗口: 触发 WM_DESTROY"
    };

    int y = 20;
    for (const wchar_t* line : lines)
    {
        TextOut(hdc, 20, y, line, lstrlenW(line));
        y += 25;
    }

    // 绘制客户区尺寸信息
    std::wstringstream ss;
    ss << L"客户区尺寸: " << rect.right << L" x " << rect.bottom;
    std::wstring sizeText = ss.str();
    TextOutW(hdc, 20, rect.bottom - 40, sizeText.c_str(), (int)sizeText.length());

    // 绘制分隔线
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, 10, rect.bottom - 50, nullptr);
    LineTo(hdc, rect.right - 10, rect.bottom - 50);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    EndPaint(hwnd, &ps);

    return 0;
}

/**
 * 处理 WM_LBUTTONDOWN 消息
 *
 * 当用户在客户区按下鼠标左键时发送。
 *
 * 参数说明:
 *   hwnd   - 窗口句柄
 *   flags  - 按键状态 (MK_CONTROL, MK_SHIFT 等)
 *   x      - 鼠标 x 坐标
 *   y      - 鼠标 y 坐标
 */
LRESULT OnLButtonDown(HWND hwnd, UINT flags, int x, int y)
{
    // 使用 LOWORD/HIWORD 宏从 lParam 中提取坐标
    // 或者使用 GET_X_LPARAM/GET_Y_LPARAM 宏 (推荐用于多平台兼容)

    HDC hdc = GetDC(hwnd);

    // 绘制点击位置的标记
    Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);

    // 显示坐标信息
    std::wstringstream ss;
    ss << L"点击位置: (" << x << L", " << y << L")";

    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(255, 255, 255));

    RECT rect;
    GetClientRect(hwnd, &rect);
    rect.top = rect.bottom - 35;
    rect.bottom = rect.bottom - 10;
    rect.left = 20;
    rect.right = 300;

    DrawTextW(hdc, ss.str().c_str(), -1, &rect, DT_LEFT);

    ReleaseDC(hwnd, hdc);

    return 0;
}

/**
 * 处理 WM_KEYDOWN 消息
 *
 * 当用户按下非系统键时发送。
 *
 * 参数说明:
 *   hwnd  - 窗口句柄
 *   vkCode - 虚拟键码 (VK_*)
 */
LRESULT OnKeyDown(HWND hwnd, UINT vkCode)
{
    // 获取按键名称
    wchar_t keyName[32];
    UINT scanCode [[maybe_unused]] = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);

    // 处理特殊键
    switch (vkCode)
    {
        case VK_LEFT:
            wcscpy_s(keyName, L"VK_LEFT (Left Arrow)");
            break;
        case VK_RIGHT:
            wcscpy_s(keyName, L"VK_RIGHT (Right Arrow)");
            break;
        case VK_UP:
            wcscpy_s(keyName, L"VK_UP (Up Arrow)");
            break;
        case VK_DOWN:
            wcscpy_s(keyName, L"VK_DOWN (Down Arrow)");
            break;
        case VK_RETURN:
            wcscpy_s(keyName, L"VK_RETURN (Enter)");
            break;
        case VK_SPACE:
            wcscpy_s(keyName, L"VK_SPACE (Space)");
            break;
        case VK_ESCAPE:
            wcscpy_s(keyName, L"VK_ESCAPE (Esc)");
            break;
        default:
            if (vkCode >= 'A' && vkCode <= 'Z')
            {
                wsprintfW(keyName, L"'%C' (0x%02X)", vkCode, vkCode);
            }
            else if (vkCode >= '0' && vkCode <= '9')
            {
                wsprintfW(keyName, L"'%c' (0x%02X)", vkCode, vkCode);
            }
            else
            {
                wsprintfW(keyName, L"Virtual Key 0x%02X", vkCode);
            }
            break;
    }

    // 显示按键信息
    HDC hdc = GetDC(hwnd);
    std::wstringstream ss;
    ss << L"按键: " << keyName;

    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(255, 255, 255));

    RECT rect;
    GetClientRect(hwnd, &rect);
    rect.top = rect.bottom - 60;
    rect.bottom = rect.bottom - 40;
    rect.left = 20;
    rect.right = 400;

    DrawTextW(hdc, ss.str().c_str(), -1, &rect, DT_LEFT);

    ReleaseDC(hwnd, hdc);

    return 0;
}

/**
 * 处理 WM_DESTROY 消息
 *
 * 当窗口被销毁时发送。
 * 这是进行清理工作的机会。
 */
LRESULT OnDestroy(HWND hwnd)
{
    // 向消息队列投递 WM_QUIT 消息
    // 这会导致 GetMessage 返回 0，从而退出消息循环
    PostQuitMessage(0);

    return 0;
}

/**
 * 注册窗口类
 */
BOOL RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};

    // 设置结构体大小
    wc.cbSize = sizeof(WNDCLASSEX);

    // 窗口样式
    wc.style = CS_HREDRAW | CS_VREDRAW;  // 宽度或高度变化时重绘

    // 关键：指向窗口过程的函数指针
    wc.lpfnWndProc = WindowProc;

    // 额外类和窗口字节 (用于存储类/窗口特定的额外数据)
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;

    // 应用程序实例句柄
    wc.hInstance = hInstance;

    // 图标 (使用系统图标)
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    // 光标 (使用箭头光标)
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // 背景画刷 (使用系统白色画刷)
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 菜单名称 (无菜单)
    wc.lpszMenuName = nullptr;

    // 窗口类名称
    wc.lpszClassName = WINDOW_CLASS_NAME;

    // 注册窗口类
    return RegisterClassEx(&wc);
}

/**
 * 创建并显示主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance)
{
    // 窗口标题
    const wchar_t* WINDOW_TITLE = L"Window Procedure 演示";

    // 计算窗口大小 (以客户区尺寸为基础)
    int clientWidth = 600;
    int clientHeight = 400;

    RECT rect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,                          // 扩展样式
        WINDOW_CLASS_NAME,          // 窗口类名称
        WINDOW_TITLE,               // 窗口标题
        WS_OVERLAPPEDWINDOW,        // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,  // 位置 (x, y)
        windowWidth, windowHeight,  // 大小
        nullptr,                    // 父窗口句柄
        nullptr,                    // 菜单句柄
        hInstance,                  // 应用程序实例句柄
        nullptr                     // 额外参数
    );

    return hwnd;
}

/**
 * WinMain - Windows 应用程序入口点
 *
 * 参数说明:
 *   hInstance     - 当前应用程序实例句柄
 *   hPrevInstance - Win32 中总是为 nullptr
 *   lpCmdLine     - 命令行参数字符串
 *   nCmdShow      - 窗口显示方式 (SW_SHOW, SW_SHOWMAXIMIZED 等)
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR lpCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    if (!RegisterWindowClass(hInstance))
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateMainWindow(hInstance);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 显示并更新窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);  // 转换键盘消息
        DispatchMessage(&msg);   // 分发消息到窗口过程
    }

    // 返回退出代码
    return (int)msg.wParam;
}
