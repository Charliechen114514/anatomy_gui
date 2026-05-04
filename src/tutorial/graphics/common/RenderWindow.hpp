/**
 * RenderWindow.hpp - 渲染窗口基类
 *
 * 为 GPU 渲染示例（Direct2D / D3D11 / D3D12 / OpenGL）提供：
 * - PeekMessage 消息循环（持续渲染）
 * - WM_SIZE / WM_PAINT / WM_DESTROY 标准处理
 * - 虚函数：OnCreate / OnResize / Render / OnDestroy
 *
 * 用法：class MyRenderer : public RenderWindow { ... };
 */

#pragma once

#include <windows.h>

template <typename Derived>
class RenderWindow
{
public:
    HWND GetHwnd() const noexcept { return m_hwnd; }
    bool IsRunning() const noexcept { return m_running; }

    // 注册窗口类并创建窗口
    bool Create(
        HINSTANCE hInstance,
        const wchar_t* className,
        const wchar_t* title,
        int width = 800,
        int height = 600,
        DWORD style = WS_OVERLAPPEDWINDOW)
    {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProcStub;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        RegisterClassEx(&wc);

        RECT rc = { 0, 0, width, height };
        AdjustWindowRect(&rc, style, FALSE);

        m_hwnd = CreateWindowEx(
            0, className, title, style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, this);

        return m_hwnd != nullptr;
    }

    // PeekMessage 渲染循环（适合 GPU 渲染）
    int RunMessageLoop()
    {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        MSG msg = {};
        while (m_running)
        {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    m_running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (m_running)
            {
                static_cast<Derived*>(this)->Render();
            }
        }
        return (int)msg.wParam;
    }

protected:
    // 子类需要实现的虚函数
    // void OnCreate();
    // void OnResize(int width, int height);
    // void Render();
    // void OnDestroy();

    HWND m_hwnd = nullptr;
    bool m_running = true;
    int m_width = 0;
    int m_height = 0;

private:
    static LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<RenderWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (uMsg)
        {
        case WM_NCCREATE:
        {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_CREATE:
            static_cast<Derived*>(self)->OnCreate();
            return 0;

        case WM_SIZE:
        {
            self->m_width = LOWORD(lParam);
            self->m_height = HIWORD(lParam);
            if (self->m_width > 0 && self->m_height > 0)
                static_cast<Derived*>(self)->OnResize(self->m_width, self->m_height);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            static_cast<Derived*>(self)->OnDestroy();
            self->m_running = false;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
};
