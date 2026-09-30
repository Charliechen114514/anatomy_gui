// M1a · 具象 —— 第一节：把窗口立起来
//
// 这一段只有一个目标：让一个 Windows 程序站住。
// 双击能开、能拖能缩、点 X 干净退出。别的什么都不干。
//
// 刻意不做的事（第 3 节会回来算账，现在别急着补）：
//   · 没声明 DPI 感知 —— 150% 缩放下整个窗口会被系统拉伸，字是糊的
//   · 没挂 manifest —— 于是既没有 DPI 声明，也没有视觉样式
//   · 没有任何封装 —— 状态就该是全局的，消息就该是一个大 switch

#include <windows.h>

// 窗口类名。注册窗口类时用一次，创建窗口时再用一次，所以提出来。
constexpr wchar_t kClassName[] = L"AnatomyGuiM1a";

// 窗口过程：这个程序收到的每一条消息的最后落脚点。
// static：它只在本文件里被系统回调，不需要外部链接。
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        // 窗口销毁 ≠ 进程退出。把 WM_QUIT 投进消息队列，让 GetMessage 返回 0，
        // 循环才会结束 —— 这两件事是分开的。
        PostQuitMessage(0);
        return 0;

    default:
        // 不处理的消息必须交回系统，否则窗口连标题栏和边框都画不出来。
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 1) 注册窗口类：告诉系统"这种窗口长什么样、消息交给谁"。
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;       // 消息的落脚点
    wc.hInstance = hInstance;       // 本模块的实例句柄
    wc.lpszClassName = kClassName;  // 类名，进程内唯一
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    if (!RegisterClassExW(&wc))
        return 1;

    // 2) 创建窗口。此时它还没被 ShowWindow，所以还看不见。
    HWND hwnd = CreateWindowExW(
        0, kClassName, L"Anatomy GUI · M1a",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 3) 消息循环：整个程序的心跳。
    //    GetMessage 返回 0 = 收到 WM_QUIT；返回 -1 = 出错。
    //    写成 while (GetMessage(...)) 会把 -1 也当成"继续"—— 经典坑。
    MSG msg{};
    while (true)
    {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0)
            break;      // WM_QUIT
        if (r == -1)
            return 1;   // 出错

        TranslateMessage(&msg);  // 把按键消息翻译成字符消息
        DispatchMessageW(&msg);  // 投给窗口过程
    }

    return static_cast<int>(msg.wParam);
}
