/**
 * 01_hello_world - Win32 Hello World 示例程序
 *
 * 这是最基础的 Win32 GUI 程序，展示了创建 Windows 窗口的基本步骤：
 * 1. 定义窗口过程函数 (WindowProc) - 处理窗口消息
 * 2. 注册窗口类 (RegisterClass) - 告诉系统窗口的属性
 * 3. 创建窗口 (CreateWindowEx) - 创建实际的窗口实例
 * 4. 消息循环 (GetMessage/TranslateMessage/DispatchMessage) - 处理消息
 *
 * 参考: tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md
 */

#ifndef UNICODE
#define UNICODE  // 使用 Unicode 字符集
#endif

#include <windows.h>

// 窗口类名称 - 必须是唯一的
static const wchar_t* WINDOW_CLASS_NAME = L"HelloWorldClass";

/**
 * 窗口过程函数 (Window Procedure)
 *
 * 这是 Win32 程序的核心函数，所有发送到窗口的消息都会通过这个函数处理。
 * 系统会调用这个函数来通知窗口各种事件。
 *
 * 参数说明:
 *   hwnd    - 窗口句柄，标识接收消息的窗口
 *   uMsg    - 消息标识符，表示消息的类型 (如 WM_PAINT, WM_DESTROY 等)
 *   wParam  - 消息的附加参数，具体含义取决于消息类型
 *   lParam  - 消息的附加参数，具体含义取决于消息类型
 *
 * 返回值:
 *   处理结果取决于消息类型。对于大多数消息，返回 0 表示已处理。
 *   未处理的消息应该返回 DefWindowProc 的结果。
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_DESTROY - 窗口销毁消息
     *
     * 当窗口被销毁时发送此消息。
     * 这是程序退出的时机 - 调用 PostQuitMessage 向消息队列投递 WM_QUIT 消息，
     * 这会导致 GetMessage 返回 0，从而退出消息循环。
     */
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    /**
     * WM_PAINT - 绘制消息
     *
     * 当窗口客户区需要重绘时发送此消息。
     * 这是进行所有绘图操作的入口。
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区矩形
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 绘制 "Hello, World!" 文本
        // DT_CENTER: 水平居中
        // DT_VCENTER: 垂直居中
        // DT_SINGLELINE: 单行文本
        DrawText(
            hdc,
            L"Hello, World!",           // 要绘制的文本
            -1,                         // 字符串长度 (-1 表示自动计算)
            &rect,                      // 绘制区域
            DT_CENTER | DT_VCENTER | DT_SINGLELINE  // 对齐方式
        );

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        // 对于我们不处理的消息，交给系统的默认处理函数
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

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
    // 初始化 WNDCLASSEX 结构体
    WNDCLASSEX wc = {};

    // 设置结构体大小
    wc.cbSize = sizeof(WNDCLASSEX);

    // 窗口样式
    // CS_HREDRAW: 宽度变化时重绘整个窗口
    // CS_VREDRAW: 高度变化时重绘整个窗口
    wc.style = CS_HREDRAW | CS_VREDRAW;

    // 窗口过程函数指针 - 这是最重要的成员
    wc.lpfnWndProc = WindowProc;

    // 额外的类和窗口字节数（用于存储自定义数据）
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;

    // 应用程序实例句柄
    wc.hInstance = hInstance;

    // 图标 - 使用系统默认的应用程序图标
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    // 光标 - 使用系统默认的箭头光标
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // 背景画刷 - 使用系统默认的窗口颜色
    // (COLOR_WINDOW + 1) 是获取系统窗口颜色的标准方式
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 菜单名称 - 本例没有菜单
    wc.lpszMenuName = nullptr;

    // 窗口类名称 - 必须唯一
    wc.lpszClassName = WINDOW_CLASS_NAME;

    // 注册窗口类
    return RegisterClassEx(&wc);
}

/**
 * 创建主窗口
 *
 * 创建并显示应用程序的主窗口。
 *
 * 参数:
 *   hInstance - 应用程序实例句柄
 *   nCmdShow  - 窗口显示状态 (如 SW_SHOW, SW_SHOWMAXIMIZED 等)
 *
 * 返回:
 *   成功返回窗口句柄，失败返回 nullptr
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,                          // 扩展窗口样式 (0 表示无扩展样式)
        WINDOW_CLASS_NAME,          // 窗口类名称 (必须是已注册的类)
        L"Hello World",             // 窗口标题 (显示在标题栏)
        WS_OVERLAPPEDWINDOW,        // 窗口样式 (标准重叠窗口，有标题栏、边框、菜单栏等)
        CW_USEDEFAULT, CW_USEDEFAULT,  // 窗口位置 (x, y) - CW_USEDEFAULT 让系统决定
        500, 400,                   // 窗口大小 (宽度, 高度)
        nullptr,                    // 父窗口句柄 (nullptr 表示没有父窗口)
        nullptr,                    // 菜单句柄 (nullptr 表示使用窗口类菜单)
        hInstance,                  // 应用程序实例句柄
        nullptr                     // 创建参数 (传递给 WM_CREATE 消息的 lParam)
    );

    if (hwnd == nullptr)
    {
        return nullptr;
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);

    // 更新窗口 (触发 WM_PAINT 消息)
    UpdateWindow(hwnd);

    return hwnd;
}

/**
 * wWinMain - Windows 应用程序入口点 (Unicode 版本)
 *
 * 这是 Windows GUI 程序的入口函数，相当于控制台程序的 main 函数。
 *
 * 参数说明:
 *   hInstance     - 当前应用程序的实例句柄
 *                   用于标识程序加载到内存中的位置
 *   hPrevInstance - 前一个实例的句柄 (Win32 中始终为 nullptr)
 *                   这是 16 位 Windows 遗留的参数，现在已废弃
 *   pCmdLine      - 命令行参数字符串 (Unicode 版本)
 *   nCmdShow      - 窗口初始显示状态
 *                   如 SW_SHOW, SW_HIDE, SW_SHOWMAXIMIZED, SW_SHOWMINIMIZED 等
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
    // 这是 Windows 程序的核心 - 不断从消息队列中获取消息并处理
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        // TranslateMessage - 转换键盘消息
        // 将虚拟键消息转换为字符消息 (WM_KEYDOWN -> WM_CHAR)
        TranslateMessage(&msg);

        // DispatchMessage - 分发消息到窗口过程
        // 系统会调用 WindowProc 函数来处理消息
        DispatchMessage(&msg);
    }

    // 步骤 4: 退出程序
    // 当 GetMessage 收到 WM_QUIT 消息时返回 0，退出循环
    // msg.wParam 包含了 PostQuitMessage 指定的退出码
    return (int)msg.wParam;
}
