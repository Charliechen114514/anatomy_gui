/**
 * 10_oop_wrapper - 面向对象封装示例
 *
 * 本示例展示如何使用 BaseWindow 模板类实现面向对象的 Win32 窗口编程。
 *
 * 这是 MFC、ATL、WTL 等 Windows 框架的核心设计原理：
 * 1. 使用 CRTP (奇异递归模板模式) 将 this 指针与窗口关联
 * 2. 通过模板基类封装窗口创建和消息循环
 * 3. 派生类只需实现消息处理逻辑
 *
 * 参考: tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md
 *       "管理应用程序状态 -> 方案三：面向对象封装"
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include "../common/BaseWindow.hpp"

/**
 * MainWindow - 主窗口类
 *
 * 继承 BaseWindow<MainWindow>，使用 CRTP 模式。
 * 这样基类可以通过 DERIVED_TYPE 类型指针调用派生类的函数。
 */
class MainWindow : public BaseWindow<MainWindow>
{
public:
    /**
     * 返回窗口类名
     * 用于注册窗口类，每个窗口类需要唯一的名称
     */
    PCWSTR ClassName() const override
    {
        return L"MainWindowClass";
    }

    /**
     * 消息处理函数
     *
     * 这是虚函数，由基类的 WindowProc 静态函数调用。
     * 所有窗口消息都会通过这个函数分发处理。
     *
     * @param uMsg 消息类型
     * @param wParam 消息参数（字相关）
     * @param lParam 消息参数（长相关）
     * @return 消息处理结果
     */
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        switch (uMsg)
        {
        case WM_DESTROY:
            // 窗口销毁消息
            // 退出消息循环，结束应用程序
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
        {
            // 绘制消息
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);

            // 获取客户区大小
            RECT rect;
            GetClientRect(m_hwnd, &rect);

            // 绘制文本
            const std::wstring text = L"Hello, OOP World!";
            const std::wstring subtext = L"Using BaseWindow template class";

            DrawText(hdc, text.c_str(), -1, &rect,
                     DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);

            // 在下方绘制子文本
            rect.top += 30;
            DrawText(hdc, subtext.c_str(), -1, &rect,
                     DT_SINGLELINE | DT_CENTER | DT_TOP | DT_NOCLIP);

            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            // 鼠标左键按下消息
            m_clickCount++;
            InvalidateRect(m_hwnd, nullptr, TRUE);  // 触发重绘
            return 0;
        }

        default:
            // 其他消息使用默认处理
            return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
        }
    }

private:
    int m_clickCount = 0;  // 点击计数器
};

/**
 * 程序入口点
 *
 * 这是 Unicode 版本的 WinMain 入口函数。
 *
 * @param hInstance 当前程序实例句柄
 * @param hPrevInstance 前一个实例句柄（Win32 中始终为 NULL）
 * @param lpCmdLine 命令行参数字符串
 * @param nCmdShow 窗口显示状态（SW_SHOW, SW_HIDE 等）
 * @return 程序退出代码
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR lpCmdLine,
    int nCmdShow)
{
    // 1. 创建主窗口对象
    MainWindow win;

    // 2. 创建窗口
    // Create 方法内部会：
    //   - 注册窗口类（首次创建时）
    //   - 调用 CreateWindowEx 创建窗口
    //   - 将 this 指针保存到窗口的用户数据中
    if (win.Create(L"10 OOP Wrapper - BaseWindow Template") == nullptr)
    {
        return 0;
    }

    // 3. 显示和更新窗口
    ShowWindow(win.Window(), nCmdShow);
    UpdateWindow(win.Window());

    // 4. 消息循环
    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);  // 翻译键盘消息
        DispatchMessage(&msg);   // 分发消息到窗口过程
    }

    return 0;
}
