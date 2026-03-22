#pragma once

#include <windows.h>
#include <stdexcept>

/**
 * BaseWindow - 窗口基类模板
 *
 * 提供面向对象的 Win32 窗口封装，使 C++ 类能够接收窗口消息。
 *
 * 使用方法:
 * 1. 继承此类: class MainWindow : public BaseWindow<MainWindow>
 * 2. 实现 HandleMessage 虚函数
 * 3. 调用 Create() 创建窗口
 *
 * 参考: tutorial/native_win32/1_ProgrammingGUI_NativeWindows.md
 *       "管理应用程序状态 -> 方案三：面向对象封装"
 */
template <class DERIVED_TYPE>
class BaseWindow
{
public:
    /**
     * 静态窗口过程函数 - 供 Win32 API 调用
     * 负责在 WM_NCCREATE 时保存 this 指针，并在其他消息时恢复
     */
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        DERIVED_TYPE* pThis = nullptr;

        if (uMsg == WM_NCCREATE)
        {
            // 窗口创建时，从 CREATESTRUCT 中提取 this 指针
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<DERIVED_TYPE*>(pCreate->lpCreateParams);

            // 将 this 指针保存在窗口的用户数据中
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));

            // 保存窗口句柄
            pThis->m_hwnd = hwnd;
        }
        else
        {
            // 其他消息时，从窗口的用户数据中恢复 this 指针
            pThis = reinterpret_cast<DERIVED_TYPE*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis)
        {
            // 调用派生类的消息处理函数
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        else
        {
            // this 指针尚未设置，使用默认处理
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    /**
     * 获取窗口句柄
     */
    HWND Window() const { return m_hwnd; }

    /**
     * 获取窗口类名（由派生类实现）
     */
    virtual PCWSTR ClassName() const = 0;

    /**
     * 创建窗口
     *
     * @param windowName 窗口标题
     * @param style 窗口样式（默认 WS_OVERLAPPEDWINDOW）
     * @param x 窗口 X 位置（默认 CW_USEDEFAULT）
     * @param y 窗口 Y 位置（默认 CW_USEDEFAULT）
     * @param width 窗口宽度（默认 CW_USEDEFAULT）
     * @param height 窗口高度（默认 CW_USEDEFAULT）
     * @param hWndParent 父窗口句柄（默认 NULL）
     * @param hMenu 菜单句柄（默认 NULL）
     */
    HWND Create(
        PCWSTR windowName,
        DWORD style = WS_OVERLAPPEDWINDOW,
        DWORD exStyle = 0,
        int x = CW_USEDEFAULT,
        int y = CW_USEDEFAULT,
        int width = CW_USEDEFAULT,
        int height = CW_USEDEFAULT,
        HWND hWndParent = nullptr,
        HMENU hMenu = nullptr)
    {
        // 注册窗口类
        RegisterWindowClass();

        // 创建窗口
        m_hwnd = CreateWindowEx(
            exStyle,
            ClassName(),           // 窗口类名
            windowName,            // 窗口标题
            style,                 // 窗口样式
            x, y, width, height,   // 位置和大小
            hWndParent,            // 父窗口
            hMenu,                 // 菜单
            GetModuleHandle(nullptr),  // 实例句柄
            this                   // 传递 this 指针
        );

        return m_hwnd;
    }

protected:
    /**
     * 消息处理函数 - 由派生类实现
     */
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

    /**
     * 窗口句柄
     */
    HWND m_hwnd = nullptr;

private:
    /**
     * 注册窗口类
     * 使用懒加载方式，只在首次创建窗口时注册
     */
    void RegisterWindowClass()
    {
        WNDCLASSEX wc = { };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;  // 窗口大小改变时重绘
        wc.lpfnWndProc = DERIVED_TYPE::WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = ClassName();

        RegisterClassEx(&wc);
    }
};
