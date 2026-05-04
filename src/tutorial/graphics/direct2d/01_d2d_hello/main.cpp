/**
 * 01_d2d_hello - Direct2D 入门示例
 *
 * 本程序演示了 Direct2D 的基础用法：
 * 1. D2D1CreateFactory           — 创建 Direct2D 工厂（单线程）
 * 2. CreateHwndRenderTarget      — 创建绑定到 Win32 HWND 的渲染目标
 * 3. CreateSolidColorBrush       — 创建纯色画刷（多种颜色）
 * 4. BeginDraw / EndDraw         — 渲染循环
 * 5. DrawRectangle / FillRectangle — 绘制和填充矩形
 * 6. DrawEllipse                 — 绘制椭圆
 * 7. DrawLine                    — 绘制直线
 * 8. D2DERR_RECREATE_TARGET      — 设备丢失处理
 *
 * 本例使用简单的 WndProc 模式（不使用 RenderWindow 模板），
 * 适合入门学习 Direct2D 的基本流程。
 *
 * 参考: tutorial/native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <algorithm>
#include <d2d1.h>
#include <cmath>

#include "ComHelper.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D2DHelloClass";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 应用程序状态 — 保存 Direct2D 资源
// ============================================================================

struct AppState
{
    // Direct2D 工厂对象 — 所有 D2D 资源的创建源头
    ComPtr<ID2D1Factory>          pFactory;

    // 渲染目标 — 绑定到窗口 HWND，用于在窗口上绘制
    ComPtr<ID2D1HwndRenderTarget> pRenderTarget;

    // 纯色画刷 — 用于填充和描边
    ComPtr<ID2D1SolidColorBrush>  pBlackBrush;     // 黑色画刷（描边）
    ComPtr<ID2D1SolidColorBrush>  pRedBrush;       // 红色画刷
    ComPtr<ID2D1SolidColorBrush>  pBlueBrush;      // 蓝色画刷
    ComPtr<ID2D1SolidColorBrush>  pGreenBrush;     // 绿色画刷
    ComPtr<ID2D1SolidColorBrush>  pOrangeBrush;    // 橙色画刷
    ComPtr<ID2D1SolidColorBrush>  pGrayBrush;      // 灰色画刷（背景）
    ComPtr<ID2D1SolidColorBrush>  pWhiteBrush;     // 白色画刷

    // 动画角度（用于旋转线条演示）
    float animAngle = 0.0f;
};

// ============================================================================
// Direct2D 资源创建
// ============================================================================

/**
 * 创建 Direct2D 工厂和渲染目标
 *
 * Direct2D 使用"工厂"模式创建所有资源：
 * - ID2D1Factory 是顶层工厂对象，负责创建渲染目标和其他资源
 * - ID2D1HwndRenderTarget 是绑定到窗口的渲染目标
 *
 * 工厂类型选择：
 * - D2D1_FACTORY_TYPE_SINGLE_THREADED: 单线程，性能更好
 * - D2D1_FACTORY_TYPE_MULTI_THREADED:  多线程安全，有额外开销
 */
static HRESULT CreateD2DResources(AppState* state, HWND hwnd)
{
    HRESULT hr;

    // --------------------------------------------------------------------------
    // 1. 创建 Direct2D 工厂（单线程模式）
    // --------------------------------------------------------------------------
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,   // 单线程模式（性能优先）
        state->pFactory.GetAddressOf());
    if (FAILED(hr)) return hr;

    // --------------------------------------------------------------------------
    // 2. 获取窗口客户区大小，创建 HwndRenderTarget
    // --------------------------------------------------------------------------
    RECT rc;
    GetClientRect(hwnd, &rc);

    // 渲染目标属性
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.pixelFormat = D2D1::PixelFormat(
        DXGI_FORMAT_UNKNOWN,              // 使用窗口默认像素格式
        D2D1_ALPHA_MODE_PREMULTIPLIED);   // 预乘 Alpha

    // HwndRenderTarget 属性
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        hwnd,
        D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top),
        D2D1_PRESENT_OPTIONS_NONE);       // 等待 VSync

    hr = state->pFactory->CreateHwndRenderTarget(
        rtProps, hwndProps, state->pRenderTarget.GetAddressOf());
    if (FAILED(hr)) return hr;

    // --------------------------------------------------------------------------
    // 3. 创建纯色画刷
    // --------------------------------------------------------------------------
    // 黑色描边画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), state->pBlackBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 红色画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Red), state->pRedBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 蓝色画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.18f, 0.55f, 0.92f, 1.0f), state->pBlueBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 绿色画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.30f, 0.69f, 0.31f, 1.0f), state->pGreenBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 橙色画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Orange), state->pOrangeBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 浅灰色画刷（背景填充）
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f), state->pGrayBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    // 白色画刷
    hr = state->pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), state->pWhiteBrush.GetAddressOf());
    if (FAILED(hr)) return hr;

    return S_OK;
}

/**
 * 丢弃渲染目标及相关画刷（用于设备丢失后重建）
 *
 * 当 GPU 设备丢失（如分辨率变更、驱动更新等），
 * 渲染目标会返回 D2DERR_RECREATE_TARGET。
 * 此时需要释放旧的渲染目标，在下一帧重新创建。
 */
static void DiscardD2DResources(AppState* state)
{
    state->pRenderTarget.Reset();
    state->pBlackBrush.Reset();
    state->pRedBrush.Reset();
    state->pBlueBrush.Reset();
    state->pGreenBrush.Reset();
    state->pOrangeBrush.Reset();
    state->pGrayBrush.Reset();
    state->pWhiteBrush.Reset();
}

// ============================================================================
// 绘制函数
// ============================================================================

/**
 * 绘制示例内容
 *
 * Direct2D 绘制流程：
 * 1. BeginDraw() — 开始绘制
 * 2. 设置变换矩阵（可选）
 * 3. Clear() — 清除背景
 * 4. 各种 DrawXxx/FillXxx 调用
 * 5. EndDraw() — 结束绘制并呈现
 */
static HRESULT DrawScene(AppState* state, HWND hwnd)
{
    HRESULT hr = S_OK;

    // 如果渲染目标尚未创建，先创建
    if (!state->pRenderTarget)
    {
        hr = CreateD2DResources(state, hwnd);
        if (FAILED(hr)) return hr;
    }

    // 获取客户区大小
    RECT rc;
    GetClientRect(hwnd, &rc);
    float width  = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);

    // --------------------------------------------------------------------------
    // 开始绘制 — 必须与 EndDraw 配对
    // --------------------------------------------------------------------------
    state->pRenderTarget->BeginDraw();

    // 清除背景 — 使用浅灰色
    state->pRenderTarget->Clear(D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f));

    // --------------------------------------------------------------------------
    // 区域 1: 填充矩形 (FillRectangle)
    // --------------------------------------------------------------------------
    {
        // 白色面板背景
        D2D1_RECT_F panelRect = D2D1::RectF(20.0f, 20.0f, width / 2.0f - 10.0f, height / 2.0f - 10.0f);
        state->pRenderTarget->FillRectangle(&panelRect, state->pWhiteBrush);

        // 内部填充不同颜色的矩形
        float padding = 15.0f;
        float rectW = (panelRect.right - panelRect.left - padding * 4) / 3.0f;
        float rectH = (panelRect.bottom - panelRect.top - padding * 3 - 30.0f) / 2.0f;
        if (rectW < 10.0f) rectW = 10.0f;
        if (rectH < 10.0f) rectH = 10.0f;

        // 第一行矩形
        D2D1_RECT_F redRect = D2D1::RectF(
            panelRect.left + padding,
            panelRect.top + 40.0f,
            panelRect.left + padding + rectW,
            panelRect.top + 40.0f + rectH);
        state->pRenderTarget->FillRectangle(&redRect, state->pRedBrush);
        state->pRenderTarget->DrawRectangle(&redRect, state->pBlackBrush, 1.0f);

        D2D1_RECT_F blueRect = D2D1::RectF(
            redRect.right + padding,
            panelRect.top + 40.0f,
            redRect.right + padding + rectW,
            panelRect.top + 40.0f + rectH);
        state->pRenderTarget->FillRectangle(&blueRect, state->pBlueBrush);
        state->pRenderTarget->DrawRectangle(&blueRect, state->pBlackBrush, 1.0f);

        D2D1_RECT_F greenRect = D2D1::RectF(
            blueRect.right + padding,
            panelRect.top + 40.0f,
            blueRect.right + padding + rectW,
            panelRect.top + 40.0f + rectH);
        state->pRenderTarget->FillRectangle(&greenRect, state->pGreenBrush);
        state->pRenderTarget->DrawRectangle(&greenRect, state->pBlackBrush, 1.0f);

        // 第二行矩形 — 不同颜色
        D2D1_RECT_F orangeRect = D2D1::RectF(
            panelRect.left + padding,
            redRect.bottom + padding,
            panelRect.left + padding + rectW,
            redRect.bottom + padding + rectH);
        state->pRenderTarget->FillRectangle(&orangeRect, state->pOrangeBrush);
        state->pRenderTarget->DrawRectangle(&orangeRect, state->pBlackBrush, 1.0f);

        // 面板边框
        state->pRenderTarget->DrawRectangle(&panelRect, state->pBlackBrush, 1.5f);
    }

    // --------------------------------------------------------------------------
    // 区域 2: 椭圆 (DrawEllipse)
    // --------------------------------------------------------------------------
    {
        D2D1_RECT_F panelRect = D2D1::RectF(width / 2.0f + 10.0f, 20.0f, width - 20.0f, height / 2.0f - 10.0f);
        state->pRenderTarget->FillRectangle(&panelRect, state->pWhiteBrush);

        float cx = (panelRect.left + panelRect.right) / 2.0f;
        float cy = (panelRect.top + panelRect.bottom) / 2.0f + 15.0f;
        float rx = (panelRect.right - panelRect.left) / 4.0f;
        float ry = (panelRect.bottom - panelRect.top - 40.0f) / 4.0f;
        if (rx < 15.0f) rx = 15.0f;
        if (ry < 15.0f) ry = 15.0f;

        // 绘制三个重叠的椭圆（D2D 自带抗锯齿，效果比 GDI 好很多）
        D2D1_ELLIPSE ellipse1 = D2D1::Ellipse(
            D2D1::Point2F(cx - rx * 0.4f, cy - ry * 0.2f), rx * 0.8f, ry * 0.8f);
        state->pRenderTarget->FillEllipse(&ellipse1, state->pRedBrush);
        state->pRenderTarget->DrawEllipse(&ellipse1, state->pBlackBrush, 1.0f);

        D2D1_ELLIPSE ellipse2 = D2D1::Ellipse(
            D2D1::Point2F(cx + rx * 0.4f, cy - ry * 0.2f), rx * 0.8f, ry * 0.8f);
        state->pRenderTarget->FillEllipse(&ellipse2, state->pBlueBrush);
        state->pRenderTarget->DrawEllipse(&ellipse2, state->pBlackBrush, 1.0f);

        D2D1_ELLIPSE ellipse3 = D2D1::Ellipse(
            D2D1::Point2F(cx, cy + ry * 0.35f), rx * 0.8f, ry * 0.8f);
        state->pRenderTarget->FillEllipse(&ellipse3, state->pGreenBrush);
        state->pRenderTarget->DrawEllipse(&ellipse3, state->pBlackBrush, 1.0f);

        // 面板边框
        state->pRenderTarget->DrawRectangle(&panelRect, state->pBlackBrush, 1.5f);
    }

    // --------------------------------------------------------------------------
    // 区域 3: 直线绘制 (DrawLine) — 旋转射线
    // --------------------------------------------------------------------------
    {
        D2D1_RECT_F panelRect = D2D1::RectF(20.0f, height / 2.0f + 10.0f, width / 2.0f - 10.0f, height - 20.0f);
        state->pRenderTarget->FillRectangle(&panelRect, state->pWhiteBrush);

        float cx = (panelRect.left + panelRect.right) / 2.0f;
        float cy = (panelRect.top + panelRect.bottom) / 2.0f + 10.0f;
        float lineLen = std::min(panelRect.right - panelRect.left, panelRect.bottom - panelRect.top) / 2.0f - 30.0f;
        if (lineLen < 20.0f) lineLen = 20.0f;

        // 绘制旋转射线（使用动画角度）
        int numRays = 12;
        for (int i = 0; i < numRays; ++i)
        {
            float angle = state->animAngle + i * (2.0f * 3.14159265f / numRays);
            float ex = cx + lineLen * cosf(angle);
            float ey = cy + lineLen * sinf(angle);

            // 交替使用不同颜色的线条
            ID2D1SolidColorBrush* pBrush = (i % 3 == 0) ? state->pRedBrush :
                                           (i % 3 == 1) ? state->pBlueBrush :
                                                          state->pGreenBrush;
            state->pRenderTarget->DrawLine(
                D2D1::Point2F(cx, cy),
                D2D1::Point2F(ex, ey),
                pBrush, 2.5f);
        }

        // 中心点
        D2D1_ELLIPSE centerDot = D2D1::Ellipse(D2D1::Point2F(cx, cy), 5.0f, 5.0f);
        state->pRenderTarget->FillEllipse(&centerDot, state->pOrangeBrush);
        state->pRenderTarget->DrawEllipse(&centerDot, state->pBlackBrush, 1.0f);

        // 面板边框
        state->pRenderTarget->DrawRectangle(&panelRect, state->pBlackBrush, 1.5f);
    }

    // --------------------------------------------------------------------------
    // 区域 4: 描边矩形 + 填充矩形组合 (DrawRectangle / FillRectangle)
    // --------------------------------------------------------------------------
    {
        D2D1_RECT_F panelRect = D2D1::RectF(width / 2.0f + 10.0f, height / 2.0f + 10.0f, width - 20.0f, height - 20.0f);
        state->pRenderTarget->FillRectangle(&panelRect, state->pWhiteBrush);

        float cx = (panelRect.left + panelRect.right) / 2.0f;
        float cy = (panelRect.top + panelRect.bottom) / 2.0f + 10.0f;

        // 绘制嵌套矩形 — 从外到内
        float size = std::min(panelRect.right - panelRect.left, panelRect.bottom - panelRect.top) / 2.0f - 20.0f;
        if (size < 30.0f) size = 30.0f;

        ID2D1SolidColorBrush* colors[] = {
            state->pRedBrush, state->pOrangeBrush,
            state->pBlueBrush, state->pGreenBrush
        };

        int numBoxes = 4;
        for (int i = 0; i < numBoxes; ++i)
        {
            float s = size * (1.0f - i * 0.2f);
            D2D1_RECT_F boxRect = D2D1::RectF(cx - s, cy - s, cx + s, cy + s);

            // 填充 + 描边
            state->pRenderTarget->FillRectangle(&boxRect, colors[i]);
            state->pRenderTarget->DrawRectangle(&boxRect, state->pBlackBrush, 1.5f);
        }

        // 面板边框
        state->pRenderTarget->DrawRectangle(&panelRect, state->pBlackBrush, 1.5f);
    }

    // --------------------------------------------------------------------------
    // 结束绘制
    // --------------------------------------------------------------------------
    hr = state->pRenderTarget->EndDraw();

    // 处理设备丢失 — 如果渲染目标需要重建，释放所有资源
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardD2DResources(state);
        return S_OK;  // 下一帧会自动重建
    }

    return hr;
}

// ============================================================================
// 窗口过程
// ============================================================================

/**
 * 简单的 WndProc — 不使用 RenderWindow 模板，适合入门理解
 *
 * Direct2D 使用持续渲染（PeekMessage 循环），
 * 但这里使用 WM_PAINT + 定时器触发重绘的简单方式。
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppState* state = reinterpret_cast<AppState*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建应用程序状态
        state = new AppState();
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        // 创建 Direct2D 资源
        HRESULT hr = CreateD2DResources(state, hwnd);
        ThrowIfFailed(hr, "CreateD2DResources 失败");

        // 启动定时器，用于动画（30fps）
        SetTimer(hwnd, 1, 33, nullptr);
        return 0;
    }

    case WM_DESTROY:
    {
        // 停止定时器
        KillTimer(hwnd, 1);

        // 释放 Direct2D 资源
        if (state)
        {
            DiscardD2DResources(state);
            state->pFactory.Reset();
            delete state;
        }
        PostQuitMessage(0);
        return 0;
    }

    case WM_TIMER:
    {
        // 更新动画角度
        if (wParam == 1 && state)
        {
            state->animAngle += 0.03f;
            if (state->animAngle > 2.0f * 3.14159265f)
                state->animAngle -= 2.0f * 3.14159265f;

            // 触发重绘
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SIZE:
    {
        // 窗口大小改变时，调整渲染目标大小
        if (state && state->pRenderTarget)
        {
            UINT width  = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            state->pRenderTarget->Resize(D2D1::SizeU(width, height));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_PAINT:
    {
        // Direct2D 绘制
        if (state)
        {
            HRESULT hr = DrawScene(state, hwnd);
            if (FAILED(hr) && hr != D2DERR_RECREATE_TARGET)
            {
                ThrowIfFailed(hr, "DrawScene 失败");
            }
        }

        // 验证窗口区域（防止 WM_PAINT 反复触发）
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        // 阻止 GDI 擦除背景 — Direct2D 自己管理背景
        return 1;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

static ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // Direct2D 自己管理背景
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 程序入口
// ============================================================================

/**
 * WinMain — Windows GUI 程序入口
 *
 * Direct2D 基于 COM，需要调用 CoInitializeEx 初始化 COM 库。
 * 退出时调用 CoUninitialize 清理。
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // --------------------------------------------------------------------------
    // 初始化 COM 库（Direct2D 依赖 COM）
    // COINIT_APARTMENTTHREADED 对应单线程工厂
    // --------------------------------------------------------------------------
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ThrowIfFailed(hr, "CoInitializeEx 失败");

    // 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"Direct2D \x5165\x95E8\x793A\x4F8B",   // "入门示例"
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 450,
        nullptr, nullptr, hInstance, nullptr);

    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // --------------------------------------------------------------------------
    // 消息循环
    // 这里使用 GetMessage（简单模式）。
    // 对于高性能动画，应使用 PeekMessage 持续渲染循环。
    // --------------------------------------------------------------------------
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理 COM
    CoUninitialize();

    return (int)msg.wParam;
}
