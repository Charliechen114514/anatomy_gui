// 52_advanced_messages - 高级输入消息综合演示：触控画板
// 对应教程第 52 篇：WM_TOUCH / WM_WINDOWPOSCHANGING / WM_PRINTCLIENT / WM_DISPLAYCHANGE
//
// 综合演示功能：
//   - 鼠标（WM_LBUTTONDOWN/MOUSEMOVE/UP）与触控（WM_TOUCH）双输入绘制笔画
//   - R / G / B 切换当前颜色，C 清空画布
//   - WM_WINDOWPOSCHANGING：限制窗口最小尺寸 400×300
//   - WM_PRINTCLIENT：让截图工具 / PrintWindow 能正确截取客户区
//   - WM_DISPLAYCHANGE：分辨率变化时把窗口拉回可见区域

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   // Windows 7：WM_TOUCH / RegisterTouchWindow
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>
#include <windowsx.h>          // GET_X_LPARAM / GET_Y_LPARAM
#include <vector>
#include <utility>             // std::move

// ============================================================================
// 笔画数据结构
// ============================================================================
struct StrokePoint
{
    int x, y;
    int pressure;              // 0-1024（仅笔输入有意义，本示例保留字段）
};

struct Stroke
{
    std::vector<StrokePoint> points;
    COLORREF color;
};

// ============================================================================
// 全局绘制状态
// ============================================================================
static std::vector<Stroke> g_strokes;
static Stroke              g_currentStroke;
static bool                g_drawing = false;
static COLORREF            g_currentColor = RGB(0, 0, 0);

// 把一条折线作为一条完整笔画加入已完成列表
static void AddPolyline(std::vector<StrokePoint> pts, COLORREF color)
{
    Stroke s;
    s.points = std::move(pts);
    s.color  = color;
    g_strokes.push_back(std::move(s));
}

// 启动时预置几条示例笔画，让窗口一打开就有内容。
// 注意：这只是为了演示展示，实际应用中笔画由用户通过鼠标/触控绘制。
static void SeedDemoStrokes()
{
    // 基于 800×600 窗口（客户区约 800×560）画一个彩色三角形
    const int Ax = 250, Ay = 480;   // 左下
    const int Bx = 550, By = 480;   // 右下
    const int Cx = 400, Cy = 80;    // 顶点

    auto line = [](int x0, int y0, int x1, int y1, COLORREF color) {
        std::vector<StrokePoint> pts;
        const int N = 48;
        for (int i = 0; i <= N; ++i)
            pts.push_back({ x0 + (x1 - x0) * i / N,
                            y0 + (y1 - y0) * i / N, 0 });
        AddPolyline(std::move(pts), color);
    };

    line(Ax, Ay, Bx, By, RGB(220,  50,  50));   // 红：底边
    line(Bx, By, Cx, Cy, RGB( 50, 160,  60));   // 绿：右边
    line(Cx, Cy, Ax, Ay, RGB( 50,  90, 220));   // 蓝：左边
}

// ============================================================================
// 绘制：WM_PAINT 与 WM_PRINTCLIENT 共用同一份逻辑
// ============================================================================
static void DrawContent(HWND hWnd, HDC hdc, const RECT& rc)
{
    // 白色背景
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    // 绘制所有已完成笔画
    for (const auto& stroke : g_strokes)
    {
        if (stroke.points.size() < 2) continue;

        HPEN hPen = CreatePen(PS_SOLID, 3, stroke.color);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);

        MoveToEx(hdc, stroke.points[0].x, stroke.points[0].y, nullptr);
        for (size_t i = 1; i < stroke.points.size(); ++i)
            LineTo(hdc, stroke.points[i].x, stroke.points[i].y);

        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }
}

// ============================================================================
// 窗口过程
// ============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        // 注册触控输入：否则触控会被系统翻译成鼠标消息，收不到 WM_TOUCH
        RegisterTouchWindow(hWnd, TWF_FINETOUCH);
        SeedDemoStrokes();
        return 0;

    // ---------- 鼠标输入 ----------
    case WM_LBUTTONDOWN:
        g_drawing = true;
        g_currentStroke = {};
        g_currentStroke.color = g_currentColor;
        g_currentStroke.points.push_back(
            { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0 });
        SetCapture(hWnd);
        return 0;

    case WM_MOUSEMOVE:
        if (!g_drawing) return 0;
        g_currentStroke.points.push_back(
            { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0 });
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONUP:
        if (!g_drawing) return 0;
        g_drawing = false;
        if (g_currentStroke.points.size() >= 2)
            g_strokes.push_back(g_currentStroke);
        ReleaseCapture();
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    // ---------- 触控输入（多点触控）----------
    case WM_TOUCH:
    {
        HTOUCHINPUT hTouch = (HTOUCHINPUT)lParam;
        UINT nInputs = LOWORD(wParam);
        std::vector<TOUCHINPUT> inputs(nInputs);

        if (GetTouchInputInfo(hTouch, nInputs, inputs.data(), sizeof(TOUCHINPUT)))
        {
            for (UINT i = 0; i < nInputs; ++i)
            {
                // TOUCHINPUT 坐标为百分之一像素，需除以 100
                POINT pt = { inputs[i].x / 100, inputs[i].y / 100 };
                ScreenToClient(hWnd, &pt);

                if (inputs[i].dwFlags & TOUCHEVENTF_DOWN)
                {
                    g_drawing = true;
                    g_currentStroke = {};
                    g_currentStroke.color = g_currentColor;
                    g_currentStroke.points.push_back({ pt.x, pt.y, 0 });
                }
                // 注意：& 优先级低于 &&，必须先用括号对 dwFlags 取位与
                else if (((inputs[i].dwFlags & TOUCHEVENTF_MOVE) != 0) && g_drawing)
                {
                    g_currentStroke.points.push_back({ pt.x, pt.y, 0 });
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                else if (inputs[i].dwFlags & TOUCHEVENTF_UP)
                {
                    g_drawing = false;
                    if (g_currentStroke.points.size() >= 2)
                        g_strokes.push_back(g_currentStroke);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
        }
        // 每个 WM_TOUCH 处理完必须关闭句柄
        CloseTouchInputHandle(hTouch);
        return 0;
    }

    // ---------- 窗口位置：拦截并修改 ----------
    case WM_WINDOWPOSCHANGING:
    {
        // 限制窗口最小尺寸为 400×300
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        if (wp->cx < 400) wp->cx = 400;
        if (wp->cy < 300) wp->cy = 300;
        return 0;
    }

    // ---------- 外部请求绘制客户区（截图工具 / PrintWindow）----------
    case WM_PRINTCLIENT:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        DrawContent(hWnd, hdc, rc);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);
        DrawContent(hWnd, hdc, rc);

        // 叠加绘制当前正在画的笔画
        if (g_drawing && g_currentStroke.points.size() >= 2)
        {
            HPEN hPen = CreatePen(PS_SOLID, 3, g_currentStroke.color);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            MoveToEx(hdc, g_currentStroke.points[0].x,
                     g_currentStroke.points[0].y, nullptr);
            for (size_t i = 1; i < g_currentStroke.points.size(); ++i)
                LineTo(hdc, g_currentStroke.points[i].x,
                       g_currentStroke.points[i].y);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    // ---------- 键盘：切换颜色 / 清空 ----------
    case WM_KEYDOWN:
        if (wParam == 'C')
        {
            g_strokes.clear();
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        else if (wParam == 'R') g_currentColor = RGB(220,  50,  50);
        else if (wParam == 'G') g_currentColor = RGB( 50, 160,  60);
        else if (wParam == 'B') g_currentColor = RGB( 50,  90, 220);
        return 0;

    // ---------- 显示器分辨率 / 数量变化 ----------
    case WM_DISPLAYCHANGE:
    {
        RECT rc;
        GetWindowRect(hWnd, &rc);
        int screenW = LOWORD(lParam);
        int screenH = HIWORD(lParam);
        if (rc.left > screenW || rc.top > screenH)
            SetWindowPos(hWnd, nullptr, 100, 100, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        return 0;
    }

    case WM_DESTROY:
        UnregisterTouchWindow(hWnd);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// 入口
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = L"AdvancedMessagesDemo";
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        0, L"AdvancedMessagesDemo", L"高级输入消息演示 — 触控画板",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (hWnd)
    {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}
