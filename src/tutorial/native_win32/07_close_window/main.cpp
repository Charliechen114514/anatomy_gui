/*
 * Win32 关闭窗口示例程序
 *
 * 本程序演示 Win32 窗口的关闭流程，重点展示：
 * 1. WM_CLOSE 消息处理 - 用户点击关闭按钮时触发
 * 2. WM_DESTROY 消息处理 - 窗口销毁时触发
 * 3. 完整的消息流程: WM_CLOSE -> DestroyWindow -> WM_DESTROY -> PostQuitMessage -> WM_QUIT
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"CloseWindowExample";
static const wchar_t* WINDOW_TITLE = L"Win32 关闭窗口示例";

/*
 * 窗口过程函数 - 处理所有发送到窗口的消息
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /*
     * WM_CLOSE - 关闭消息
     *
     * 触发时机:
     * - 用户点击窗口右上角的 X 按钮
     * - 用户按 Alt+F4
     * - 系统关机时
     * - 程序调用 SendMessage(hwnd, WM_CLOSE, 0, 0)
     *
     * 处理方式:
     * - 如果不处理此消息，DefWindowProc 会自动调用 DestroyWindow
     * - 如果返回 0，窗口不会被销毁（相当于取消关闭）
     * - 调用 DestroyWindow 会触发 WM_DESTROY
     */
    case WM_CLOSE:
    {
        // 弹出确认对话框
        int result = MessageBoxW(
            hwnd,                           // 父窗口句柄
            L"确定要关闭窗口吗？",         // 对话框文本
            L"确认",                        // 对话框标题
            MB_YESNO | MB_ICONQUESTION      // 按钮 | 图标
        );

        // 用户选择是否关闭窗口
        if (result == IDYES)
        {
            // 用户确认关闭 -> 销毁窗口
            // DestroyWindow 会发送 WM_DESTROY 消息
            DestroyWindow(hwnd);
        }
        // 如果用户选择"否"，只需返回 0，不做任何操作
        // 这样窗口就不会被关闭

        return 0;
    }

    /*
     * WM_DESTROY - 销毁消息
     *
     * 触发时机:
     * - DestroyWindow 被调用后
     * - 窗口正在被销毁
     *
     * 重要:
     * - 此时窗口仍然存在，但不可见
     * - 子窗口已被销毁
     * - 应该在这里释放资源（内存、GDI 对象等）
     * - 必须调用 PostQuitMessage 退出消息循环
     */
    case WM_DESTROY:
    {
        // 释放资源（示例中没有需要释放的资源）

        // PostQuitMessage 向消息队列投递 WM_QUIT 消息
        // 这会导致 GetMessage 返回 0，从而退出消息循环
        PostQuitMessage(0);

        return 0;
    }

    /*
     * WM_NCDESTROY - 非客户区销毁消息
     *
     * 触发时机:
     * - 在 WM_DESTROY 之后
     * - 窗口完全销毁后
     *
     * 这是窗口收到的最后一条消息
     */
    case WM_NCDESTROY:
    {
        // 可以在这里进行最后的清理工作
        return 0;
    }

    /*
     * WM_PAINT - 绘制消息
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区大小
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 绘制提示文字
        const wchar_t* text = L"点击右上角的 X 按钮测试关闭功能";
        DrawTextW(
            hdc,
            text,
            -1,
            &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        // 其他消息交给默认处理
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/*
 * 注册窗口类
 */
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};

    // 设置窗口类结构体大小
    wc.cbSize = sizeof(WNDCLASSEX);

    // 窗口样式 - CS_HREDRAW | CS_VREDRAW 表示宽度或高度变化时重绘
    wc.style = CS_HREDRAW | CS_VREDRAW;

    // 窗口过程函数指针
    wc.lpfnWndProc = WindowProc;

    // 实例句柄
    wc.hInstance = hInstance;

    // 图标和光标
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    // 背景色
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 类名
    wc.lpszClassName = WINDOW_CLASS_NAME;

    // 注册窗口类
    return RegisterClassExW(&wc);
}

/*
 * 创建并显示窗口
 */
HWND CreateAndShowWindow(HINSTANCE hInstance, int nCmdShow)
{
    // 创建窗口
    HWND hwnd = CreateWindowExW(
        0,                           // 扩展窗口样式
        WINDOW_CLASS_NAME,           // 窗口类名
        WINDOW_TITLE,                // 窗口标题
        WS_OVERLAPPEDWINDOW,         // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT, // 位置 (x, y)
        400, 300,                    // 大小 (宽度, 高度)
        NULL,                        // 父窗口句柄
        NULL,                        // 菜单句柄
        hInstance,                   // 实例句柄
        NULL                         // 附加数据
    );

    if (hwnd == NULL)
    {
        return NULL;
    }

    // 显示并更新窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return hwnd;
}

/*
 * wWinMain - 程序入口点 (Unicode 版本)
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR lpCmdLine,
    int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // 1. 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBoxW(NULL, L"窗口类注册失败！", L"错误", MB_ICONERROR);
        return 0;
    }

    // 2. 创建并显示窗口
    HWND hwnd = CreateAndShowWindow(hInstance, nCmdShow);
    if (hwnd == NULL)
    {
        MessageBoxW(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
        return 0;
    }

    /*
     * 3. 消息循环
     *
     * 关闭流程说明:
     *
     * 用户点击 X 按钮
     *     ↓
     * 系统发送 WM_CLOSE 消息
     *     ↓
     * WindowProc 处理 WM_CLOSE
     *     ↓
     * 弹出确认对话框 (MessageBox)
     *     ↓
     * 用户点击"是" (IDYES)
     *     ↓
     * 调用 DestroyWindow(hwnd)
     *     ↓
     * 系统发送 WM_DESTROY 消息
     *     ↓
     * WindowProc 处理 WM_DESTROY
     *     ↓
     * 调用 PostQuitMessage(0)
     *     ↓
     * 向消息队列投递 WM_QUIT 消息
     *     ↓
     * GetMessage 收到 WM_QUIT，返回 0
     *     ↓
     * 退出消息循环
     *     ↓
     * 程序结束
     */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);  // 转换键盘消息
        DispatchMessageW(&msg);   // 分发消息到窗口过程
    }

    // 返回退出码
    return (int)msg.wParam;
}
