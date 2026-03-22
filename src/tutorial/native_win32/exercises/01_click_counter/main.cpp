/**
 * Win32 Click Counter 练习程序
 *
 * 本程序演示 Win32 窗口消息处理和 GDI 绘图的基本概念：
 * 1. WM_LBUTTONDOWN 消息处理 - 左键点击增加计数
 * 2. WM_RBUTTONDOWN 消息处理 - 右键点击重置计数
 * 3. WM_PAINT 消息处理 - 绘制居中的大字体文本
 * 4. InvalidateRect 触发重绘
 * 5. 全局变量保存应用状态
 *
 * 编译方式:
 *   cmake -B build
 *   cmake --build build
 */

#include <windows.h>
#include <tchar.h>

// =============================================================================
// 全局变量 - 保存应用状态
// =============================================================================

// 点击计数
int g_clickCount = 0;

// 窗口类名
const PCWSTR WINDOW_CLASS_NAME = L"ClickCounterWindow";

// =============================================================================
// 辅助函数
// =============================================================================

/**
 * 将数字转换为宽字符串
 */
void FormatCountText(wchar_t* buffer, size_t bufferSize, int count)
{
    swprintf_s(buffer, bufferSize, L"%d", count);
}

// =============================================================================
// 窗口过程函数
// =============================================================================

/**
 * 窗口过程 - 处理窗口消息
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        // 左键点击 - 增加计数
        g_clickCount++;

        // 触发重绘 - 标记整个客户区需要重绘
        InvalidateRect(hwnd, nullptr, TRUE);

        return 0;

    case WM_RBUTTONDOWN:
        // 右键点击 - 重置计数
        g_clickCount = 0;

        // 触发重绘
        InvalidateRect(hwnd, nullptr, TRUE);

        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取客户区大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 创建大字体
        LOGFONT lf = { 0 };
        lf.lfHeight = -96;  // 负值表示字符高度，96 约等于 96pt
        lf.lfWeight = FW_BOLD;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, L"Arial");

        HFONT hFont = CreateFontIndirect(&lf);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        // 设置文本颜色和背景模式
        SetTextColor(hdc, RGB(50, 50, 50));
        SetBkMode(hdc, TRANSPARENT);

        // 绘制标题提示
        RECT rcTop = rcClient;
        rcTop.bottom = rcClient.top + 80;

        HFONT hSmallFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
        DrawText(hdc, L"Left Click: +1 | Right Click: Reset", -1, &rcTop,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 恢复大字体绘制计数
        SelectObject(hdc, hFont);

        // 绘制点击计数（居中显示）
        wchar_t countText[32];
        FormatCountText(countText, _countof(countText), g_clickCount);

        RECT rcCenter = rcClient;
        DrawText(hdc, countText, -1, &rcCenter,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 恢复原来的字体并删除创建的字体
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        // 窗口销毁 - 发送退出消息
        PostQuitMessage(0);
        return 0;
    }

    // 默认消息处理
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// =============================================================================
// 主函数
// =============================================================================

/**
 * 注册窗口类
 */
void RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = { };

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;  // 窗口大小改变时重绘
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);  // 白色背景
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassEx(&wc);
}

/**
 * WinMain - 程序入口点
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 注册窗口类
    RegisterWindowClass(hInstance);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,                          // 扩展样式
        WINDOW_CLASS_NAME,          // 窗口类名
        L"Click Counter - 点击计数器",  // 窗口标题
        WS_OVERLAPPEDWINDOW,        // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,  // 位置
        500, 400,                   // 大小
        nullptr,                    // 父窗口
        nullptr,                    // 菜单
        hInstance,                  // 实例句柄
        nullptr                     // 附加数据
    );

    if (hwnd == nullptr)
    {
        return 0;
    }

    // 显示和更新窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
