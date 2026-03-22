/*
 * Win32 鼠标追踪器
 *
 * 功能：
 * - 在窗口标题栏实时显示鼠标坐标
 * - 在窗口客户区显示当前鼠标位置
 * - 显示鼠标按键状态
 *
 * 技术要点：
 * - WM_MOUSEMOVE 消息处理
 * - GET_X_LPARAM / GET_Y_LPARAM 宏提取坐标
 * - SetWindowTextW 更新窗口标题
 * - WM_PAINT 绘制信息
 * - WM_LBUTTONDOWN / WM_LBUTTONUP 处理按键状态
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <cstdio>

// 全局状态
struct MouseState {
    int x;           // 当前鼠标 X 坐标
    int y;           // 当前鼠标 Y 坐标
    bool leftDown;   // 左键是否按下
    bool mouseMoved; // 鼠标是否移动过（用于初始化）

    MouseState() : x(0), y(0), leftDown(false), mouseMoved(false) {}
} g_mouseState;

// 更新窗口标题
void UpdateWindowTitle(HWND hwnd) {
    if (!g_mouseState.mouseMoved) {
        SetWindowTextW(hwnd, L"鼠标追踪器 - 移动鼠标以显示坐标");
        return;
    }

    wchar_t title[256];
    swprintf_s(title, L"鼠标追踪器 - X: %d, Y: %d", g_mouseState.x, g_mouseState.y);
    SetWindowTextW(hwnd, title);
}

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEMOVE: {
            // 提取鼠标坐标
            g_mouseState.x = GET_X_LPARAM(lParam);
            g_mouseState.y = GET_Y_LPARAM(lParam);
            g_mouseState.mouseMoved = true;

            // 更新窗口标题
            UpdateWindowTitle(hwnd);

            // 重绘客户区以显示新坐标
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        case WM_LBUTTONDOWN: {
            g_mouseState.leftDown = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        case WM_LBUTTONUP: {
            g_mouseState.leftDown = false;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 设置文本颜色和背景
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);

            // 获取客户区大小
            RECT rect;
            GetClientRect(hwnd, &rect);

            // 创建字体
            HFONT hFont = CreateFontW(
                24,                    // 高度
                0,                     // 宽度
                0,                     // 转角
                0,                     // 方向
                FW_NORMAL,             // 字重
                FALSE,                 // 斜体
                FALSE,                 // 下划线
                FALSE,                 // 删除线
                DEFAULT_CHARSET,       // 字符集
                OUT_DEFAULT_PRECIS,    // 输出精度
                CLIP_DEFAULT_PRECIS,   // 裁剪精度
                DEFAULT_QUALITY,       // 质量
                DEFAULT_PITCH | FF_SWISS, // 字体间距
                L"Microsoft YaHei"     // 字体名称
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            // 显示坐标信息
            wchar_t coordText[256];
            if (g_mouseState.mouseMoved) {
                swprintf_s(coordText, L"鼠标坐标: (%d, %d)", g_mouseState.x, g_mouseState.y);
            } else {
                wcscpy_s(coordText, L"鼠标坐标: 移动鼠标以显示");
            }
            TextOutW(hdc, 20, 20, coordText, (int)wcslen(coordText));

            // 显示按键状态
            wchar_t buttonText[256];
            swprintf_s(buttonText, L"左键状态: %s",
                      g_mouseState.leftDown ? L"按下" : L"释放");
            TextOutW(hdc, 20, 60, buttonText, (int)wcslen(buttonText));

            // 显示提示信息
            const wchar_t* hintText = L"提示: 在窗口内移动鼠标，点击左键";
            TextOutW(hdc, 20, 120, hintText, (int)wcslen(hintText));

            // 恢复字体并删除
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MouseTrackerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建窗口
    HWND hwnd = CreateWindowExW(
        0,
        L"MouseTrackerClass",
        L"鼠标追踪器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 300,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 显示并更新窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
