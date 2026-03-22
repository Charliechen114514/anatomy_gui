/**
 * Win32 注册窗口类示例程序
 *
 * 本程序演示如何使用 WNDCLASS 结构和 RegisterClass 函数来注册一个窗口类。
 * 窗口类是 Windows 编程中的核心概念，它定义了窗口的一系列属性和行为。
 */

#include <windows.h>
#include <tchar.h>

// 窗口类名称 - 用于标识这个窗口类
// 这个名称在创建窗口时会用到
static const TCHAR* g_szClassName = _T("MyFirstWindowClass");

/**
 * 窗口过程函数 (Window Procedure)
 *
 * 这是窗口类的"大脑"，所有发送到该类创建的窗口的消息都会由这个函数处理。
 * 它是一个回调函数，由操作系统在需要时调用。
 *
 * 参数说明:
 *   hWnd    - 窗口句柄，标识接收消息的窗口
 *   message - 消息标识符（如 WM_CREATE, WM_PAINT, WM_DESTROY 等）
 *   wParam  - 消息的第一个参数，具体含义取决于消息类型
 *   lParam  - 消息的第二个参数，具体含义取决于消息类型
 *
 * 返回值:
 *   处理结果，具体含义取决于消息类型
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        // WM_DESTROY: 窗口正在被销毁
        // PostQuitMessage 向消息队列投递一条 WM_QUIT 消息
        // 这会导致 GetMessage 返回 0，从而退出消息循环
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            // WM_PAINT: 窗口需要重绘
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // 获取客户区大小
            RECT rect;
            GetClientRect(hWnd, &rect);

            // 绘制文本
            TCHAR szText[] = _T("窗口类注册成功！这个窗口使用了自定义的窗口类。");
            DrawText(hdc, szText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hWnd, &ps);
        }
        return 0;
    }

    // 对于我们不处理的消息，交给系统默认处理
    // DefWindowProc 是默认的窗口过程函数
    return DefWindowProc(hWnd, message, wParam, lParam);
}

/**
 * 注册窗口类函数
 *
 * 封装了窗口类注册的完整流程，展示 WNDCLASS 结构的各个字段。
 *
 * 参数:
 *   hInstance - 应用程序实例句柄
 *
 * 返回值:
 *   TRUE  - 注册成功
 *   FALSE - 注册失败
 */
BOOL RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASS wc = { 0 };

    // ============================================================
    // WNDCLASS 结构字段详解
    // ============================================================

    // === 样式相关字段 ===

    // style: 窗口类样式
    // 可以使用位或 (|) 组合多个样式
    //
    // 常用样式:
    //   CS_HREDRAW     - 当窗口水平宽度改变时，重绘整个窗口
    //   CS_VREDRAW     - 当窗口垂直高度改变时，重绘整个窗口
    //   CS_DBLCLKS     - 在窗口上双击鼠标时发送双击消息
    //   CS_NOCLOSE     - 禁用系统菜单的关闭选项
    //   CS_OWNDC       - 为该类每个窗口分配一个独立的设备上下文
    //   CS_CLASSDC     - 为该类所有窗口共享一个设备上下文
    //   CS_PARENTDC    - 使用父窗口的设备上下文
    //   CS_SAVEBITS    - 保存被窗口遮挡的屏幕区域，减少重绘
    //   CS_BYTEALIGNWINDOW - 窗口在字节边界对齐（旧系统优化用）
    //   CS_BYTEALIGNCLIENT - 客户区在字节边界对齐（旧系统优化用）
    //   CS_GLOBALCLASS - 应用程序全局类（可用于其他进程）
    //
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    // === 窗口过程字段 ===

    // lpfnWndProc: 指向窗口过程函数的指针
    // 这是最重要的字段，决定了该类窗口如何处理消息
    // 必须是一个回调函数，遵循特定的函数签名
    wc.lpfnWndProc = WndProc;

    // === 额外空间字段（可选，通常设为 0） ===

    // cbClsExtra: 窗口类额外字节数
    // 用于在窗口类结构后分配额外的内存空间
    // 可以用于存储与该类所有窗口相关的自定义数据
    // 通常设置为 0，除非有特殊需求
    wc.cbClsExtra = 0;

    // cbWndExtra: 窗口实例额外字节数
    // 用于在每个窗口实例后分配额外的内存空间
    // 可以用于存储与特定窗口实例相关的自定义数据
    // 如果使用 DLGWINDOWEXTRA 对话框，需要设置为 DLGWINDOWEXTRA
    wc.cbWndExtra = 0;

    // === 资源句柄字段 ===

    // hInstance: 应用程序实例句柄
    // 标识拥有该窗口类的应用程序
    // 通常使用 WinMain 的 hInstance 参数
    // 这个句柄用于加载图标、光标等资源
    wc.hInstance = hInstance;

    // hIcon: 窗口图标句柄
    // 当窗口最小化或在任务栏显示时使用的图标
    //
    // 常用方式:
    //   NULL              - 使用默认图标
    //   LoadIcon(NULL, IDI_APPLICATION) - 系统默认应用程序图标
    //   LoadIcon(NULL, IDI_ERROR)       - 系统错误图标
    //   LoadIcon(NULL, IDI_INFORMATION) - 系统信息图标
    //   LoadIcon(NULL, IDI_WARNING)     - 系统警告图标
    //   LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON)) - 自定义图标
    //
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    // hCursor: 鼠标光标句柄
    // 当鼠标移动到该窗口上时显示的光标样式
    //
    // 常用光标:
    //   IDC_ARROW      - 标准箭头光标
    //   IDC_HAND       - 手形光标
    //   IDC_CROSS      - 十字光标
    //   IDC_IBEAM      - I 型文本输入光标
    //   IDC_WAIT       - 等待（沙漏）光标
    //   IDC_HELP       - 帮问号的光标
    //   IDC_SIZEALL    - 四向调整大小光标
    //   IDC_SIZENWSE   - 斜向调整大小光标
    //   IDC_SIZENS     - 垂直调整大小光标
    //   IDC_SIZEWE     - 水平调整大小光标
    //
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    // hbrBackground: 背景画刷句柄
    // 用于填充窗口客户区的背景颜色
    //
    // 常用方式:
    //   NULL                    - 不自动擦除背景
    //   (HBRUSH)GetStockObject(WHITE_BRUSH)     - 白色背景
    //   (HBRUSH)GetStockObject(BLACK_BRUSH)     - 黑色背景
    //   (HBRUSH)GetStockObject(GRAY_BRUSH)      - 灰色背景
    //   (HBRUSH)(COLOR_WINDOW+1)                - 系统窗口背景色
    //   (HBRUSH)(COLOR_BTNFACE+1)               - 系统按钮背景色
    //
    // 注意: 使用系统颜色时要 +1，因为颜色值转换成句柄时需要偏移
    //
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // === 菜单资源字段 ===

    // lpszMenuName: 菜单资源名称
    // 指定该窗口类默认使用的菜单资源
    //
    // 设置方式:
    //   NULL                      - 无默认菜单
    //   MAKEINTRESOURCE(IDR_MENU) - 使用整数 ID 标识的菜单资源
    //   "MyMenu"                  - 使用字符串名称标识的菜单资源
    //
    wc.lpszMenuName = NULL;

    // === 类名字段 ===

    // lpszClassName: 窗口类名称
    // 用于唯一标识这个窗口类
    // 创建窗口时需要指定这个名称
    // 必须是全局唯一的（在同一个进程中）
    //
    // 命名建议:
    //   - 使用有意义的名称
    //   - 可以包含公司名、产品名等
    //   - 示例: "MyCompany.MyProduct.MainWindow"
    //
    wc.lpszClassName = g_szClassName;

    // ============================================================
    // 注册窗口类
    // ============================================================

    // RegisterClass: 向系统注册窗口类
    //
    // 参数:
    //   指向 WNDCLASS 结构的指针
    //
    // 返回值:
    //   成功: 返回唯一的窗口类标识符（原子）
    //   失败: 返回 0，可以通过 GetLastError 获取详细错误信息
    //
    // 常见失败原因:
    //   - 类名已被注册
    //   - WNDCLASS 结构设置不正确
    //   - 系统资源不足
    //
    ATOM atom = RegisterClass(&wc);

    if (atom == 0)
    {
        // 注册失败，显示错误信息
        DWORD dwError = GetLastError();
        TCHAR szErrorMsg[256];
        wsprintf(szErrorMsg, _T("注册窗口类失败！错误代码: %d"), dwError);
        MessageBox(NULL, szErrorMsg, _T("错误"), MB_ICONERROR);
        return FALSE;
    }

    // 注册成功！atom 是这个类的唯一标识符
    // 我们可以继续使用这个类创建窗口
    return TRUE;
}

/**
 * 创建并显示主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    // 创建窗口
    // CreateWindow 的参数包括类名、窗口标题、样式、位置、大小等
    HWND hWnd = CreateWindow(
        g_szClassName,               // 窗口类名（必须是已注册的类）
        _T("Win32 窗口类注册示例"),   // 窗口标题
        WS_OVERLAPPEDWINDOW,         // 窗口样式
        CW_USEDEFAULT,               // x 坐标
        CW_USEDEFAULT,               // y 坐标
        CW_USEDEFAULT,               // 宽度
        CW_USEDEFAULT,               // 高度
        NULL,                        // 父窗口句柄
        NULL,                        // 菜单句柄
        hInstance,                   // 应用程序实例句柄
        NULL                         // 创建参数（传递给 WM_CREATE）
    );

    if (hWnd == NULL)
    {
        MessageBox(NULL, _T("创建窗口失败！"), _T("错误"), MB_ICONERROR);
        return NULL;
    }

    // 显示并更新窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

/**
 * 程序入口点
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // hInstance: 当前应用程序的实例句柄
    // hPrevInstance: Win32 中始终为 NULL（保留用于 16 位 Windows 兼容性）
    // lpCmdLine: 命令行参数字符串
    // nCmdShow: 窗口初始显示状态（最大化、最小化、正常等）

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 步骤 1: 注册窗口类
    if (!RegisterWindowClass(hInstance))
    {
        return -1;  // 注册失败，退出程序
    }

    // 步骤 2: 创建主窗口
    HWND hWnd = CreateMainWindow(hInstance, nCmdShow);
    if (hWnd == NULL)
    {
        return -1;  // 创建窗口失败，退出程序
    }

    // 步骤 3: 消息循环
    // 从消息队列中获取消息并分发给窗口过程
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // 将虚拟键消息转换为字符消息（处理键盘输入）
        TranslateMessage(&msg);

        // 将消息分发给窗口过程
        DispatchMessage(&msg);
    }

    // msg.wParam 包含程序的退出代码
    return (int)msg.wParam;
}
