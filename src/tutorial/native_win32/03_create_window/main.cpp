/*
 * Win32 创建窗口示例程序
 *
 * 本示例演示 CreateWindowEx 函数的各种参数和窗口样式
 *
 * CreateWindowEx 函数原型:
 * HWND CreateWindowExW(
 *     DWORD     dwExStyle,      // 扩展窗口样式
 *     LPCWSTR   lpClassName,    // 窗口类名 (已注册的窗口类)
 *     LPCWSTR   lpWindowName,   // 窗口标题
 *     DWORD     dwStyle,        // 窗口样式
 *     int       X,              // 窗口水平位置 (x 坐标)
 *     int       Y,              // 窗口垂直位置 (y 坐标)
 *     int       nWidth,         // 窗口宽度
 *     int       nHeight,        // 窗口高度
 *     HWND      hWndParent,     // 父窗口句柄
 *     HMENU     hMenu,          // 菜单句柄或子窗口标识符
 *     HINSTANCE hInstance,      // 应用程序实例句柄
 *     LPVOID    lpParam         // 创建参数 (传递给 WM_CREATE)
 * );
 */

#include <windows.h>
#include <string>

// =============================================================================
// 窗口过程函数 - 处理窗口消息
// =============================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        // 当窗口被销毁时，发送退出消息
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取窗口客户区大小
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 设置文本颜色和背景
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // 根据窗口类绘制不同文本
        WCHAR className[50];
        GetClassNameW(hwnd, className, 50);

        std::wstring text;
        if (wcscmp(className, L"MainWindow") == 0)
        {
            text = L"标准窗口 (WS_OVERLAPPEDWINDOW)";
        }
        else if (wcscmp(className, L"PopupWindow") == 0)
        {
            text = L"弹出窗口 (WS_POPUP)";
        }
        else if (wcscmp(className, L"ToolWindow") == 0)
        {
            text = L"工具窗口 (WS_EX_TOOLWINDOW)";
        }
        else if (wcscmp(className, L"ThinWindow") == 0)
        {
            text = L"细边框窗口 (WS_DLGFRAME)";
        }

        // 在窗口中心绘制文本
        DrawTextW(hdc, text.c_str(), -1, &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// =============================================================================
// 注册窗口类
// =============================================================================
void RegisterWindowClasses(HINSTANCE hInstance)
{
    // 主窗口类 - 使用标准窗口样式
    WNDCLASSEXW wcMain = {};
    wcMain.cbSize = sizeof(WNDCLASSEXW);
    wcMain.style = CS_HREDRAW | CS_VREDRAW;
    wcMain.lpfnWndProc = WindowProc;
    wcMain.hInstance = hInstance;
    wcMain.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcMain.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcMain.lpszClassName = L"MainWindow";
    RegisterClassExW(&wcMain);

    // 弹出窗口类
    WNDCLASSEXW wcPopup = {};
    wcPopup.cbSize = sizeof(WNDCLASSEXW);
    wcPopup.style = CS_HREDRAW | CS_VREDRAW;
    wcPopup.lpfnWndProc = WindowProc;
    wcPopup.hInstance = hInstance;
    wcPopup.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcPopup.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcPopup.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcPopup.lpszClassName = L"PopupWindow";
    RegisterClassExW(&wcPopup);

    // 工具窗口类
    WNDCLASSEXW wcTool = {};
    wcTool.cbSize = sizeof(WNDCLASSEXW);
    wcTool.style = CS_HREDRAW | CS_VREDRAW;
    wcTool.lpfnWndProc = WindowProc;
    wcTool.hInstance = hInstance;
    wcTool.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcTool.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcTool.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcTool.lpszClassName = L"ToolWindow";
    RegisterClassExW(&wcTool);

    // 细边框窗口类
    WNDCLASSEXW wcThin = {};
    wcThin.cbSize = sizeof(WNDCLASSEXW);
    wcThin.style = CS_HREDRAW | CS_VREDRAW;
    wcThin.lpfnWndProc = WindowProc;
    wcThin.hInstance = hInstance;
    wcThin.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcThin.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcThin.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcThin.lpszClassName = L"ThinWindow";
    RegisterClassExW(&wcThin);
}

// =============================================================================
// 创建标准重叠窗口 (WS_OVERLAPPEDWINDOW)
// =============================================================================
HWND CreateOverlappedWindow(HINSTANCE hInstance)
{
    /*
     * WS_OVERLAPPEDWINDOW 是最常用的窗口样式组合
     * 它实际上是以下样式的组合:
     *   WS_OVERLAPPED    | 重叠窗口 (有标题栏)
     *   WS_CAPTION       | 有标题栏
     *   WS_SYSMENU       | 有系统菜单 (左上角图标)
     *   WS_THICKFRAME    | 有可调整大小的边框
     *   WS_MINIMIZEBOX   | 有最小化按钮
     *   WS_MAXIMIZEBOX   | 有最大化按钮
     */

    return CreateWindowExW(
        0,                          // dwExStyle: 扩展样式 (0 表示无扩展样式)
        L"MainWindow",              // lpClassName: 窗口类名
        L"标准窗口 - WS_OVERLAPPEDWINDOW",  // lpWindowName: 窗口标题
        WS_OVERLAPPEDWINDOW,        // dwStyle: 窗口样式
        CW_USEDEFAULT,              // X: 水平位置 (CW_USEDEFAULT = 系统默认)
        CW_USEDEFAULT,              // Y: 垂直位置 (CW_USEDEFAULT = 系统默认)
        600,                        // nWidth: 窗口宽度 (像素)
        400,                        // nHeight: 窗口高度 (像素)
        nullptr,                    // hWndParent: 父窗口 (nullptr = 顶级窗口)
        nullptr,                    // hMenu: 菜单 (nullptr = 使用类菜单)
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 创建弹出窗口 (WS_POPUP)
// =============================================================================
HWND CreatePopupWindow(HINSTANCE hInstance, int x, int y)
{
    /*
     * WS_POPUP 创建弹出式窗口
     * 特点:
     *   - 无标题栏
     *   - 无边框 (除非添加其他样式)
     *   - 常用于对话框、菜单、启动画面
     *
     * WS_POPUPWINDOW 组合了:
     *   WS_POPUP | WS_BORDER | WS_SYSMENU
     */

    return CreateWindowExW(
        WS_EX_TOPMOST,              // dwExStyle: WS_EX_TOPMOST = 始终置顶
        L"PopupWindow",             // lpClassName: 窗口类名
        L"弹出窗口 - WS_POPUP",     // lpWindowName: 窗口标题
        WS_POPUPWINDOW,             // dwStyle: 弹出窗口样式
        x,                          // X: 指定位置
        y,                          // Y: 指定位置
        300,                        // nWidth: 窗口宽度
        200,                        // nHeight: 窗口高度
        nullptr,                    // hWndParent: 无父窗口
        nullptr,                    // hMenu: 无菜单
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 创建工具窗口 (WS_EX_TOOLWINDOW)
// =============================================================================
HWND CreateToolWindow(HINSTANCE hInstance, int x, int y)
{
    /*
     * WS_EX_TOOLWINDOW 扩展样式创建工具窗口
     * 特点:
     *   - 不在任务栏显示
     *   - 标题栏字体较小
     *   - 常用于浮动工具栏、调色板
     *
     * WS_POPUP | WS_CAPTION | WS_THICKFRAME 组合:
     *   - WS_POPUP: 弹出式
     *   - WS_CAPTION: 有标题栏
     *   - WS_THICKFRAME: 可调整大小
     */

    return CreateWindowExW(
        WS_EX_TOOLWINDOW,           // dwExStyle: 工具窗口扩展样式
        L"ToolWindow",              // lpClassName: 窗口类名
        L"工具窗口",                // lpWindowName: 窗口标题
        WS_POPUP | WS_CAPTION | WS_THICKFRAME,  // dwStyle: 组合样式
        x,                          // X: 指定位置
        y,                          // Y: 指定位置
        250,                        // nWidth: 窗口宽度
        180,                        // nHeight: 窗口高度
        nullptr,                    // hWndParent: 无父窗口
        nullptr,                    // hMenu: 无菜单
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 创建细边框窗口 (WS_DLGFRAME)
// =============================================================================
HWND CreateThinBorderWindow(HINSTANCE hInstance, int x, int y)
{
    /*
     * WS_DLGFRAME 创建对话框边框窗口
     * 特点:
     *   - 有细边框
     *   - 通常有标题栏
     *   - 不可调整大小 (无 WS_THICKFRAME)
     *
     * WS_EX_CLIENTEDGE 扩展样式:
     *   - 客户区有凹陷边框效果
     */

    return CreateWindowExW(
        WS_EX_CLIENTEDGE,           // dwExStyle: 客户区凹陷边框
        L"ThinWindow",              // lpClassName: 窗口类名
        L"细边框窗口 - WS_DLGFRAME",  // lpWindowName: 窗口标题
        WS_POPUP | WS_CAPTION | WS_DLGFRAME,  // dwStyle: 对话框样式
        x,                          // X: 指定位置
        y,                          // Y: 指定位置
        350,                        // nWidth: 窗口宽度
        150,                        // nHeight: 窗口高度
        nullptr,                    // hWndParent: 无父窗口
        nullptr,                    // hMenu: 无菜单
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 创建带上下文帮助的窗口
// =============================================================================
HWND CreateHelpWindow(HINSTANCE hInstance, int x, int y)
{
    /*
     * WS_EX_CONTEXTHELP 扩展样式:
     *   - 标题栏上有问号按钮
     *   - 点击后鼠标变为帮助选择模式
     *
     * 注意: 需要 WS_POPUP 或不包含 WS_MAXIMIZEBOX 的样式
     */

    return CreateWindowExW(
        WS_EX_CONTEXTHELP,          // dwExStyle: 帮问号按钮
        L"MainWindow",              // lpClassName: 窗口类名
        L"帮助窗口 - WS_EX_CONTEXTHELP",  // lpWindowName: 窗口标题
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,  // dwStyle: 基本重叠窗口
        x,                          // X: 指定位置
        y,                          // Y: 指定位置
        400,                        // nWidth: 窗口宽度
        300,                        // nHeight: 窗口高度
        nullptr,                    // hWndParent: 无父窗口
        nullptr,                    // hMenu: 无菜单
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 创建从右到左布局的窗口
// =============================================================================
HWND CreateRTLWindow(HINSTANCE hInstance, int x, int y)
{
    /*
     * WS_EX_LAYOUTRTL 扩展样式:
     *   - 镜像窗口布局 (用于阿拉伯语、希伯来语等 RTL 语言)
     *   - 垂直滚动条在左侧
     */

    return CreateWindowExW(
        WS_EX_LAYOUTRTL,            // dwExStyle: RTL 布局
        L"MainWindow",              // lpClassName: 窗口类名
        L"RTL 布局窗口 - WS_EX_LAYOUTRTL",  // lpWindowName: 窗口标题
        WS_OVERLAPPEDWINDOW,        // dwStyle: 标准重叠窗口
        x,                          // X: 指定位置
        y,                          // Y: 指定位置
        450,                        // nWidth: 窗口宽度
        250,                        // nHeight: 窗口高度
        nullptr,                    // hWndParent: 无父窗口
        nullptr,                    // hMenu: 无菜单
        hInstance,                  // hInstance: 应用程序实例
        nullptr                     // lpParam: 创建参数
    );
}

// =============================================================================
// 主函数 - 程序入口
// =============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册所有窗口类
    RegisterWindowClasses(hInstance);

    // 创建主窗口 (标准重叠窗口)
    HWND hwndMain = CreateOverlappedWindow(hInstance);
    if (hwndMain)
    {
        ShowWindow(hwndMain, nCmdShow);
        UpdateWindow(hwndMain);
    }

    // 获取主窗口的位置，用于定位其他窗口
    RECT mainRect;
    GetWindowRect(hwndMain, &mainRect);

    // 创建弹出窗口 (在主窗口右侧)
    HWND hwndPopup = CreatePopupWindow(hInstance,
                                       mainRect.right + 20,
                                       mainRect.top);
    if (hwndPopup)
    {
        ShowWindow(hwndPopup, SW_SHOW);
    }

    // 创建工具窗口 (在主窗口下方)
    HWND hwndTool = CreateToolWindow(hInstance,
                                     mainRect.left,
                                     mainRect.bottom + 20);
    if (hwndTool)
    {
        ShowWindow(hwndTool, SW_SHOW);
    }

    // 创建细边框窗口 (在工具窗口右侧)
    HWND hwndThin = CreateThinBorderWindow(hInstance,
                                           mainRect.left + 270,
                                           mainRect.bottom + 20);
    if (hwndThin)
    {
        ShowWindow(hwndThin, SW_SHOW);
    }

    // 创建帮助窗口 (在弹出窗口下方)
    HWND hwndHelp = CreateHelpWindow(hInstance,
                                     mainRect.right + 20,
                                     mainRect.top + 220);
    if (hwndHelp)
    {
        ShowWindow(hwndHelp, SW_SHOW);
    }

    // 创建 RTL 窗口 (在帮助窗口下方)
    HWND hwndRTL = CreateRTLWindow(hInstance,
                                   mainRect.right + 20,
                                   mainRect.top + 540);
    if (hwndRTL)
    {
        ShowWindow(hwndRTL, SW_SHOW);
    }

    // 消息循环
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

/*
 * =============================================================================
 * 窗口样式参考 (dwStyle 参数)
 * =============================================================================
 *
 * 基本窗口样式:
 *   WS_OVERLAPPED       重叠窗口 (有标题栏)
 *   WS_POPUP            弹出窗口 (不能与 WS_OVERLAPPED 同时使用)
 *   WS_CHILD            子窗口
 *   WS_MINIMIZE         初始最小化
 *   WS_VISIBLE          初始可见
 *   WS_DISABLED         初始禁用
 *   WS_CLIPSIBLINGS     裁剪子窗口 (子窗口之间)
 *   WS_CLIPCHILDREN     裁剪子窗口 (父窗口绘制时)
 *   WS_MAXIMIZE         初始最大化
 *   WS_VSCROLL          垂直滚动条
 *   WS_HSCROLL          水平滚动条
 *   WS_GROUP            组控件 (单选按钮)
 *   WS_TABSTOP          Tab 键停止
 *
 * 控制外观:
 *   WS_BORDER           单边框
 *   WS_DLGFRAME         对话框边框 (不可调整大小)
 *   WS_CAPTION          标题栏 (包含 WS_BORDER)
 *   WS_THICKFRAME       可调整大小的边框
 *   WS_SYSMENU          系统菜单
 *   WS_MINIMIZEBOX      最小化按钮
 *   WS_MAXIMIZEBOX      最大化按钮
 *
 * 预定义组合:
 *   WS_OVERLAPPEDWINDOW = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
 *                         WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
 *   WS_POPUPWINDOW = WS_POPUP | WS_BORDER | WS_SYSMENU
 *
 * =============================================================================
 * 扩展窗口样式参考 (dwExStyle 参数)
 * =============================================================================
 *
 *   WS_EX_DLGMODALFRAME   双边框 (可与 WS_THICKFRAME 组合)
 *   WS_EX_NOPARENTNOTIFY  创建/销毁时不通知父窗口
 *   WS_EX_TOPMOST         顶级窗口 (始终置顶)
 *   WS_EX_ACCEPTFILES    接受拖放文件
 *   WS_EX_TRANSPARENT     透明窗口
 *   WS_EX_MDICHILD        MDI 子窗口
 *   WS_EX_TOOLWINDOW      工具窗口 (不在任务栏显示)
 *   WS_EX_WINDOWEDGE      有凸起边框
 *   WS_EX_CLIENTEDGE      有凹陷边框
 *   WS_EX_CONTEXTHELP     帮问号按钮
 *   WS_EX_RIGHT           右对齐文本
 *   WS_EX_RTLREADING      RTL 阅读顺序
 *   WS_EX_LEFTSCROLLBAR   垂直滚动条在左侧
 *   WS_EX_CONTROLPARENT   容器窗口 (Tab 键导航)
 *   WS_EX_STATICEDGE      3D 边框样式
 *   WS_EX_APPWINDOW       在任务栏显示 (顶层窗口)
 *   WS_EX_LAYERED         分层窗口 (支持透明度)
 *   WS_EX_LAYOUTRTL       RTL 镜像布局
 *
 * =============================================================================
 * 位置和大小参数
 * =============================================================================
 *
 * X, Y:
 *   CW_USEDEFAULT - 使用系统默认位置
 *   对于子窗口，相对于父窗口客户区
 *   对于顶层窗口，屏幕坐标
 *
 * nWidth, nHeight:
 *   CW_USEDEFAULT - 使用系统默认大小
 *   单位为像素
 *
 * =============================================================================
 * hWndParent 参数
 * =============================================================================
 *
 *   nullptr         - 顶级窗口 (无父窗口)
 *   窗口句柄        - 指定父窗口
 *
 * 对于子窗口 (WS_CHILD)，必须指定父窗口
 * 对于弹出窗口，可以指定父窗口 (用于拥有关系)
 *
 * =============================================================================
 * hMenu 参数
 * =============================================================================
 *
 *   nullptr         - 使用窗口类中的菜单
 *   菜单句柄        - 窗口菜单
 *
 * 对于子窗口:
 *   - hMenu 是子窗口标识符 (整数 ID)
 *
 * =============================================================================
 * lpParam 参数
 * =============================================================================
 *
 *   - 通过 CREATESTRUCT 结构传递给 WM_CREATE
 *   - 常用于传递创建数据的指针
 *   - 可通过 LPCREATESTRUCT 的 lpCreateParams 访问
 */
