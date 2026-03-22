/*
 * Win32 消息循环示例程序
 *
 * 本程序演示 Windows 消息循环的工作原理，包括:
 * - 消息队列
 * - GetMessage / TranslateMessage / DispatchMessage 三个核心函数
 * - 窗口过程处理消息
 * - 队列消息与非队列消息的区别
 */

#include <windows.h>
#include <cstdio>

// 窗口类名称
const wchar_t CLASS_NAME[] = L"MessageLoopWindow";

/*
 * 窗口过程函数 (Window Procedure)
 *
 * 这是窗口的消息处理函数，所有发送到该窗口的消息都会在这里处理。
 *
 * 重要概念:
 * - 队列消息 (Queued Messages): 通过 GetMessage 从消息队列中获取
 *   - 鼠标/键盘输入 (WM_*BUTTON*, WM_KEY*, WM_CHAR*)
 *   - 定时器消息 (WM_TIMER)
 *   - 绘制消息 (WM_PAINT)
 *   - 退出消息 (WM_QUIT)
 *
 * - 非队列消息 (Non-Queued Messages): 直接调用窗口过程，不经过消息队列
 *   - WM_CREATE, WM_SIZE, WM_DESTROY 等窗口管理消息
 *   - WM_SETFOCUS, WM_KILLFOCUS 等焦点消息
 *
 * CALLBACK: 调用约定，确保正确的参数传递顺序
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 用于显示消息信息的缓冲区
    char debugMsg[256];

    switch (uMsg)
    {
    /*
     * WM_CREATE - 窗口创建消息
     *
     * 非队列消息: 当窗口被创建时，Windows 直接调用窗口过程发送此消息
     * 在这里可以进行一次性初始化操作
     */
    case WM_CREATE:
    {
        sprintf_s(debugMsg, sizeof(debugMsg), "[WindowProc] WM_CREATE - 窗句柄: 0x%p\n", hwnd);
        OutputDebugStringA(debugMsg);
        return 0;
    }

    /*
     * WM_PAINT - 绘制消息
     *
     * 队列消息: 当窗口需要重绘时进入消息队列
     * 触发条件: 窗口大小改变、被其他窗口遮挡后显示、调用 InvalidateRect 等
     */
    case WM_PAINT:
    {
        OutputDebugStringA("[WindowProc] WM_PAINT - 开始绘制\n");

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区大小
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 绘制文本
        const wchar_t* text = L"消息循环示例\n点击鼠标或按键查看消息";
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        OutputDebugStringA("[WindowProc] WM_PAINT - 绘制完成\n");
        return 0;
    }

    /*
     * WM_LBUTTONDOWN - 鼠标左键按下消息
     *
     * 队列消息: 鼠标点击产生的输入消息
     * wParam: 表示按键状态和辅助键状态
     * lParam: 低16位是x坐标，高16位是y坐标
     */
    case WM_LBUTTONDOWN:
    {
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);
        sprintf_s(debugMsg, sizeof(debugMsg), "[WindowProc] WM_LBUTTONDOWN - 位置: (%d, %d)\n", xPos, yPos);
        OutputDebugStringA(debugMsg);

        // 在点击位置绘制一个简单反馈
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    /*
     * WM_KEYDOWN - 键盘按下消息
     *
     * 队列消息: 键盘输入产生的消息
     * wParam: 虚拟键码 (VK_*)，如 VK_RETURN, VK_ESCAPE 等
     */
    case WM_KEYDOWN:
    {
        sprintf_s(debugMsg, sizeof(debugMsg), "[WindowProc] WM_KEYDOWN - 虚拟键码: 0x%X\n", wParam);
        OutputDebugStringA(debugMsg);

        // ESC 键退出
        if (wParam == VK_ESCAPE)
        {
            OutputDebugStringA("[WindowProc] ESC 键按下，准备关闭窗口\n");
            DestroyWindow(hwnd);
        }
        return 0;
    }

    /*
     * WM_DESTROY - 窗口销毁消息
     *
     * 非队列消息: 当窗口被销毁时发送
     * 在这里进行清理工作，最重要的是发送 WM_QUIT 消息
     *
     * PostQuitMessage: 向消息队列投递一条 WM_QUIT 消息
     * 这会导致 GetMessage 返回 FALSE，从而退出消息循环
     */
    case WM_DESTROY:
    {
        OutputDebugStringA("[WindowProc] WM_DESTROY - 窗口即将销毁\n");
        PostQuitMessage(0);
        return 0;
    }

    /*
     * 默认消息处理
     *
     * 对于我们不处理的消息，交给系统默认处理
     * DefWindowProc 处理所有我们未处理的窗口消息
     */
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

/*
 * 创建主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance)
{
    // 注册窗口类
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;      // 窗口过程函数
    wc.hInstance = hInstance;          // 实例句柄
    wc.lpszClassName = CLASS_NAME;     // 窗口类名

    // 设置窗口光标和背景
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 注册窗口类
    RegisterClass(&wc);

    // 创建窗口
    return CreateWindowEx(
        0,                          // 扩展样式
        CLASS_NAME,                 // 窗口类名
        L"Win32 消息循环示例",      // 窗口标题
        WS_OVERLAPPEDWINDOW,        // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,  // 位置和大小
        nullptr,                    // 父窗口
        nullptr,                    // 菜单
        hInstance,                  // 实例句柄
        nullptr                     // 附加数据
    );
}

/*
 * 消息循环核心函数说明:
 *
 * 1. GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
 *    - 从消息队列中获取消息
 *    - 参数:
 *      - lpMsg: 接收消息的 MSG 结构指针
 *      - hWnd: 窗口句柄，NULL 表示获取属于调用线程的所有窗口消息
 *      - wMsgFilterMin/Max: 消息过滤范围，0 表示不过滤
 *    - 返回值:
 *      - TRUE: 成功获取消息（WM_QUT 除外）
 *      - FALSE: 获取到 WM_QUIT 消息
 *      - -1: 发生错误
 *    - 特性: 阻塞函数，当消息队列为空时会等待，不会占用 CPU
 *
 * 2. TranslateMessage(const MSG *lpMsg)
 *    - 将虚拟键消息转换为字符消息
 *    - WM_KEYDOWN / WM_KEYUP -> WM_CHAR
 *    - 不是必须的，但用于处理字符输入时很有用
 *    - 返回值: 成功返回非零值
 *
 * 3. DispatchMessage(const MSG *lpMsg)
 *    - 将消息分发到窗口过程
 *    - 内部调用 WindowProc(hwnd, message, wParam, lParam)
 *    - 返回值: 返回窗口过程的返回值
 *
 * 消息流转过程:
 *    1. 用户操作（点击、按键等）产生硬件事件
 *    2. 驱动程序将事件转换为消息，放入系统消息队列
 *    3. Windows 将消息从系统队列复制到线程消息队列
 *    4. GetMessage 从线程队列获取消息
 *    5. TranslateMessage 进行消息转换（可选）
 *    6. DispatchMessage 调用窗口过程处理消息
 *    7. WindowProc 处理消息并返回结果
 */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine [[maybe_unused]], int nCmdShow)
{
    // 启用控制台输出（用于调试，可选）
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    printf("=== Win32 消息循环示例程序 ===\n");
    printf("按 ESC 键或关闭窗口退出\n\n");
    printf("使用 DebugView 或 Visual Studio 输出窗口查看详细消息\n");

    // 创建主窗口
    HWND hwnd = CreateMainWindow(hInstance);
    if (hwnd == nullptr)
    {
        return 0;
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);

    // 消息循环
    MSG msg = {};
    printf("进入消息循环...\n");
    OutputDebugStringA("[MessageLoop] 进入消息循环\n");

    /*
     * 标准消息循环
     *
     * GetMessage: 从消息队列获取消息，阻塞等待
     * 当收到 WM_QUIT 时返回 FALSE，退出循环
     */
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        /*
         * TranslateMessage: 虚拟键到字符消息的转换
         * 将 WM_KEYDOWN 转换为 WM_CHAR
         * 这样可以方便处理字符输入
         */
        TranslateMessage(&msg);

        /*
         * DispatchMessage: 分发消息到窗口过程
         * 这里会调用 WindowProc 函数
         */
        DispatchMessage(&msg);
    }

    printf("退出消息循环，退出码: %llu\n", (unsigned long long)msg.wParam);
    OutputDebugStringA("[MessageLoop] 退出消息循环\n");

    // 返回退出码
    return (int)msg.wParam;
}

/*
 * 消息循环的其他形式:
 *
 * 1. PeekMessage 循环 (非阻塞):
 *    while (true) {
 *        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
 *            if (msg.message == WM_QUIT) break;
 *            TranslateMessage(&msg);
 *            DispatchMessage(&msg);
 *        }
 *        // 在这里执行其他任务（游戏循环、动画等）
 *    }
 *
 * 2. 带加速键的消息循环:
 *    while (GetMessage(&msg, nullptr, 0, 0)) {
 *        if (TranslateAccelerator(hwnd, hAccelTable, &msg)) {
 *            continue;  // 加速键已处理，跳过 Translate/Dispatch
 *        }
 *        TranslateMessage(&msg);
 *        DispatchMessage(&msg);
 *    }
 *
 * 3. 对话框消息循环 (IsDialogMessage):
 *    while (GetMessage(&msg, nullptr, 0, 0)) {
 *        if (IsDialogMessage(hDlg, &msg)) {
 *            continue;  // 对话框已处理
 *        }
 *        TranslateMessage(&msg);
 *        DispatchMessage(&msg);
 *    }
 */
