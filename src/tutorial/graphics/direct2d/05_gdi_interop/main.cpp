/**
 * 05_gdi_interop - Direct2D GDI 互操作示例
 *
 * 本程序演示了 Direct2D 与 GDI 的互操作能力：
 * 1. ID2D1GdiInteropRenderTarget::GetDC / ReleaseDC
 *    — 在 D2D 渲染目标上获取 HDC，混用 GDI 绘图
 * 2. ID2D1DCRenderTarget
 *    — 创建绑定到任意 HDC 的 D2D 渲染目标
 * 3. 混合绘制模式：
 *    - 左半区: GDI 绘图（传统风格，锯齿明显）
 *    - 右半区: D2D 绘图（抗锯齿，高质量）
 *    - 中央: 在同一 BeginDraw/EndDraw 中混合 GDI + D2D
 *
 * 互操作的意义：
 * - 逐步迁移旧 GDI 代码到 Direct2D
 * - 利用 GDI 特有功能（如复杂文字布局）
 * - 在 D2D 渲染中嵌入 GDI 绘制的内容
 *
 * 参考: tutorial/native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>

#include "ComHelper.hpp"
#include "RenderWindow.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// D2DGdiInteropDemo — GDI 互操作演示
// ============================================================================

class D2DGdiInteropDemo : public RenderWindow<D2DGdiInteropDemo>
{
public:
    ~D2DGdiInteropDemo()
    {
        DiscardResources();
        m_pFactory.Reset();
    }

    void OnCreate()
    {
        ThrowIfFailed(
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory),
            "D2D1CreateFactory 失败");

        CreateResources();
    }

    void OnResize(int width, int height)
    {
        if (m_pRenderTarget)
        {
            m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        }
    }

    void OnDestroy()
    {
        DiscardResources();
        m_pFactory.Reset();
    }

    void Render()
    {
        if (!m_pRenderTarget)
        {
            if (FAILED(CreateResources())) return;
        }

        HRESULT hr = S_OK;
        m_pRenderTarget->BeginDraw();

        float width  = static_cast<float>(m_width);
        float height = static_cast<float>(m_height);
        float halfW  = width / 2.0f;
        float headerH = 45.0f;

        // 清除背景
        m_pRenderTarget->Clear(D2D1::ColorF(0.96f, 0.96f, 0.96f, 1.0f));

        // ------------------------------------------------------------------
        // 顶部标题栏（纯 D2D 绘制）
        // ------------------------------------------------------------------
        {
            D2D1_RECT_F headerRect = D2D1::RectF(0.0f, 0.0f, width, headerH);
            m_pRenderTarget->FillRectangle(&headerRect, m_pHeaderBgBrush);

            // 左侧标题: "GDI"
            D2D1_RECT_F gdiLabel = D2D1::RectF(10.0f, 5.0f, halfW - 10.0f, headerH - 5.0f);
            m_pRenderTarget->DrawText(
                L"GDI \x7ED8\x5236 (GDI Drawing)", 14,
                m_pLabelFormat.Get(), gdiLabel, m_pHeaderTextBrush,
                D2D1_DRAW_TEXT_OPTIONS_NONE);

            // 右侧标题: "D2D"
            D2D1_RECT_F d2dLabel = D2D1::RectF(halfW + 10.0f, 5.0f, width - 10.0f, headerH - 5.0f);
            m_pRenderTarget->DrawText(
                L"D2D \x7ED8\x5236 (D2D Drawing)", 14,
                m_pLabelFormat.Get(), d2dLabel, m_pHeaderTextBrush,
                D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // 中间分隔线
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(halfW, headerH),
            D2D1::Point2F(halfW, height),
            m_pDividerBrush, 2.0f);

        // ------------------------------------------------------------------
        // 左半区: GDI 绘制（传统风格）
        //
        // 使用 ID2D1GdiInteropRenderTarget::GetDC 获取 HDC，
        // 然后使用 GDI 函数（Rectangle, Ellipse, TextOut 等）绘制。
        // 绘制完成后调用 ReleaseDC 释放 HDC。
        // ------------------------------------------------------------------
        DrawGDIRegion(headerH, halfW, height);

        // ------------------------------------------------------------------
        // 右半区: D2D 绘制（抗锯齿高质量）
        //
        // 使用标准的 D2D 绘图 API：
        // - FillRectangle / DrawRectangle — 矩形
        // - FillEllipse / DrawEllipse     — 椭圆
        // - DrawLine                      — 直线
        // ------------------------------------------------------------------
        DrawD2DRegion(halfW, headerH, halfW, height);

        // ------------------------------------------------------------------
        // 中央: 混合渲染
        //
        // 在同一个 BeginDraw/EndDraw 中混合 GDI 和 D2D 绘制。
        // 先用 GDI 画矩形框，再用 D2D 画圆形和文字。
        // ------------------------------------------------------------------
        DrawMixedRegion(width, height);

        hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            DiscardResources();
        }
    }

private:
    ComPtr<ID2D1Factory>          m_pFactory;
    ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
    ComPtr<ID2D1GdiInteropRenderTarget> m_pGdiInterop;  // GDI 互操作接口

    // D2D 画刷
    ComPtr<ID2D1SolidColorBrush>  m_pRedBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pBlueBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pGreenBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pOrangeBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pBlackBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pWhiteBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pHeaderBgBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pHeaderTextBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pDividerBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pMixedBgBrush;

    // 文字格式
    ComPtr<IDWriteTextFormat>     m_pLabelFormat;
    ComPtr<IDWriteTextFormat>     m_pSmallFormat;

    // DirectWrite 工厂（用于文字格式）
    ComPtr<IDWriteFactory>        m_pDWriteFactory;

    HRESULT CreateResources()
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        // ------------------------------------------------------------------
        // 创建 HwndRenderTarget
        //
        // 重要：要使用 GDI 互操作，渲染目标必须支持 GDI 兼容模式。
        // 设置 pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED
        // 或 D2D1_ALPHA_MODE_IGNORE，并设置 renderTargetUsage 为
        // D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE。
        // ------------------------------------------------------------------
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        rtProps.pixelFormat = D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,              // 必须使用 BGRA 格式
            D2D1_ALPHA_MODE_PREMULTIPLIED);           // 预乘 Alpha
        rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;  // 启用 GDI 互操作

        HRESULT hr = m_pFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(
                m_hwnd,
                D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            &m_pRenderTarget);
        if (FAILED(hr)) return hr;

        // 查询 GDI 互操作接口
        hr = m_pRenderTarget->QueryInterface(
            __uuidof(ID2D1GdiInteropRenderTarget),
            reinterpret_cast<void**>(m_pGdiInterop.GetAddressOf()));
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建画刷
        // ------------------------------------------------------------------
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.90f, 0.30f, 0.24f, 1.0f), &m_pRedBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.20f, 0.55f, 0.85f, 1.0f), &m_pBlueBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.30f, 0.69f, 0.31f, 1.0f), &m_pGreenBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Orange), &m_pOrangeBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black), &m_pBlackBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White), &m_pWhiteBrush);
        if (FAILED(hr)) return hr;

        // 标题栏背景
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.20f, 0.20f, 0.22f, 1.0f), &m_pHeaderBgBrush);
        if (FAILED(hr)) return hr;

        // 标题栏文字
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White), &m_pHeaderTextBrush);
        if (FAILED(hr)) return hr;

        // 分隔线
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f), &m_pDividerBrush);
        if (FAILED(hr)) return hr;

        // 混合区域背景
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.93f, 0.93f, 0.95f, 1.0f), &m_pMixedBgBrush);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建 DirectWrite 工厂和文字格式
        // ------------------------------------------------------------------
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_pDWriteFactory.GetAddressOf()));
        if (FAILED(hr)) return hr;

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            14.0f, L"en-us",
            &m_pLabelFormat);
        if (FAILED(hr)) return hr;

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f, L"en-us",
            &m_pSmallFormat);
        if (FAILED(hr)) return hr;

        return S_OK;
    }

    void DiscardResources()
    {
        m_pRenderTarget.Reset();
        m_pGdiInterop.Reset();
        m_pRedBrush.Reset();
        m_pBlueBrush.Reset();
        m_pGreenBrush.Reset();
        m_pOrangeBrush.Reset();
        m_pBlackBrush.Reset();
        m_pWhiteBrush.Reset();
        m_pHeaderBgBrush.Reset();
        m_pHeaderTextBrush.Reset();
        m_pDividerBrush.Reset();
        m_pMixedBgBrush.Reset();
        m_pLabelFormat.Reset();
        m_pSmallFormat.Reset();
        m_pDWriteFactory.Reset();
    }

    // ------------------------------------------------------------------
    // GDI 绘制区域
    //
    // 通过 ID2D1GdiInteropRenderTarget::GetDC 获取 HDC，
    // 使用传统 GDI 函数绘制图形和文字。
    // 注意：GDI 绘制不支持抗锯齿（与 D2D 对比明显）。
    // ------------------------------------------------------------------
    void DrawGDIRegion(float top, float right, float bottom)
    {
        if (!m_pGdiInterop) return;

        // 获取 HDC — 开始 GDI 绘制
        // D2D1_DC_INITIALIZE_MODE_COPY: 保留当前 D2D 渲染内容
        HDC hdc = nullptr;
        HRESULT hr = m_pGdiInterop->GetDC(
            D2D1_DC_INITIALIZE_MODE_COPY, &hdc);
        if (FAILED(hr) || !hdc) return;

        int savedState = SaveDC(hdc);

        // GDI 绘制区域
        int left   = 0;
        int iTop   = static_cast<int>(top);
        int iRight = static_cast<int>(right - 2);
        int iBottom = static_cast<int>(bottom);

        // ------------------------------------------------------------------
        // GDI 矩形 — 注意锯齿和粗糙边缘
        // ------------------------------------------------------------------
        {
            HBRUSH hRedBrush = CreateSolidBrush(RGB(220, 60, 50));
            HPEN hRedPen = CreatePen(PS_SOLID, 2, RGB(180, 40, 30));
            SelectObject(hdc, hRedBrush);
            SelectObject(hdc, hRedPen);
            Rectangle(hdc, 20, iTop + 15, 120, iTop + 80);
            DeleteObject(hRedBrush);
            DeleteObject(hRedPen);
        }

        {
            HBRUSH hBlueBrush = CreateSolidBrush(RGB(50, 130, 210));
            HPEN hBluePen = CreatePen(PS_SOLID, 2, RGB(30, 90, 170));
            SelectObject(hdc, hBlueBrush);
            SelectObject(hdc, hBluePen);
            Rectangle(hdc, 140, iTop + 15, 260, iTop + 80);
            DeleteObject(hBlueBrush);
            DeleteObject(hBluePen);
        }

        {
            HBRUSH hGreenBrush = CreateSolidBrush(RGB(70, 170, 75));
            HPEN hGreenPen = CreatePen(PS_SOLID, 2, RGB(40, 130, 45));
            SelectObject(hdc, hGreenBrush);
            SelectObject(hdc, hGreenPen);
            Rectangle(hdc, 280, iTop + 15, iRight - 20, iTop + 80);
            DeleteObject(hGreenBrush);
            DeleteObject(hGreenPen);
        }

        // GDI 文字标注
        SetTextColor(hdc, RGB(80, 80, 80));
        SetBkMode(hdc, TRANSPARENT);
        HFONT hLabelFont = CreateFont(
            12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        SelectObject(hdc, hLabelFont);
        TextOut(hdc, 35, iTop + 40, L"GDI Rect", 8);
        TextOut(hdc, 160, iTop + 40, L"GDI Rect", 8);
        TextOut(hdc, 300, iTop + 40, L"GDI Rect", 8);
        DeleteObject(hLabelFont);

        // ------------------------------------------------------------------
        // GDI 椭圆 — 注意锯齿
        // ------------------------------------------------------------------
        {
            int ellipseTop = iTop + 100;
            HBRUSH hYellowBrush = CreateSolidBrush(RGB(255, 193, 7));
            HPEN hYellowPen = CreatePen(PS_SOLID, 2, RGB(200, 150, 0));
            SelectObject(hdc, hYellowBrush);
            SelectObject(hdc, hYellowPen);
            Ellipse(hdc, 30, ellipseTop, 170, ellipseTop + 80);
            DeleteObject(hYellowBrush);
            DeleteObject(hYellowPen);

            SetTextColor(hdc, RGB(80, 80, 80));
            TextOut(hdc, 65, ellipseTop + 30, L"GDI Ellipse", 11);
        }

        {
            int ellipseTop = iTop + 100;
            HBRUSH hPurpleBrush = CreateSolidBrush(RGB(156, 39, 176));
            HPEN hPurplePen = CreatePen(PS_SOLID, 2, RGB(120, 20, 140));
            SelectObject(hdc, hPurpleBrush);
            SelectObject(hdc, hPurplePen);
            Ellipse(hdc, 190, ellipseTop, iRight - 30, ellipseTop + 80);
            DeleteObject(hPurpleBrush);
            DeleteObject(hPurplePen);

            SetTextColor(hdc, RGB(255, 255, 255));
            TextOut(hdc, 220, ellipseTop + 30, L"GDI Ellipse", 11);
        }

        // ------------------------------------------------------------------
        // GDI 直线
        // ------------------------------------------------------------------
        {
            int lineTop = iTop + 200;
            HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
            SelectObject(hdc, hPen);
            MoveToEx(hdc, 20, lineTop, nullptr);
            LineTo(hdc, iRight - 20, lineTop);

            HPEN hDashPen = CreatePen(PS_DASH, 2, RGB(200, 50, 50));
            SelectObject(hdc, hDashPen);
            MoveToEx(hdc, 20, lineTop + 20, nullptr);
            LineTo(hdc, iRight - 20, lineTop + 20);

            DeleteObject(hPen);
            DeleteObject(hDashPen);

            SetTextColor(hdc, RGB(80, 80, 80));
            HFONT hSmallFont = CreateFont(
                11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SelectObject(hdc, hSmallFont);
            TextOut(hdc, 20, lineTop + 30, L"GDI Line (PS_SOLID / PS_DASH)", 29);
            DeleteObject(hSmallFont);
        }

        // GDI 标签
        {
            SetTextColor(hdc, RGB(180, 60, 50));
            HFONT hTagFont = CreateFont(
                16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SelectObject(hdc, hTagFont);
            int labelY = iBottom - 50;
            TextOut(hdc, 20, labelY, L"GDI (no anti-aliasing)", 22);

            SetTextColor(hdc, RGB(130, 130, 130));
            HFONT hNoteFont = CreateFont(
                11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SelectObject(hdc, hNoteFont);
            TextOut(hdc, 20, labelY + 22, L"GetDC() -> GDI draw -> ReleaseDC()", 35);
            DeleteObject(hTagFont);
            DeleteObject(hNoteFont);
        }

        RestoreDC(hdc, savedState);

        // ReleaseDC — 结束 GDI 绘制
        // 注意：传入的 RECT 指定需要更新的区域（nullptr = 全部）
        m_pGdiInterop->ReleaseDC(nullptr);
    }

    // ------------------------------------------------------------------
    // D2D 绘制区域
    //
    // 使用 D2D API 绘制相同的图形，对比抗锯齿效果。
    // ------------------------------------------------------------------
    void DrawD2DRegion(float left, float top, float width, float height)
    {
        float right  = left + width;
        float bottom = top + height;

        // ------------------------------------------------------------------
        // D2D 矩形 — 抗锯齿平滑
        // ------------------------------------------------------------------
        {
            D2D1_RECT_F redRect = D2D1::RectF(left + 20.0f, top + 15.0f, left + 120.0f, top + 80.0f);
            m_pRenderTarget->FillRectangle(&redRect, m_pRedBrush);
            m_pRenderTarget->DrawRectangle(&redRect, m_pBlackBrush, 2.0f);

            D2D1_RECT_F blueRect = D2D1::RectF(left + 140.0f, top + 15.0f, left + 260.0f, top + 80.0f);
            m_pRenderTarget->FillRectangle(&blueRect, m_pBlueBrush);
            m_pRenderTarget->DrawRectangle(&blueRect, m_pBlackBrush, 2.0f);

            D2D1_RECT_F greenRect = D2D1::RectF(left + 280.0f, top + 15.0f, right - 20.0f, top + 80.0f);
            m_pRenderTarget->FillRectangle(&greenRect, m_pGreenBrush);
            m_pRenderTarget->DrawRectangle(&greenRect, m_pBlackBrush, 2.0f);

            // 文字标注
            D2D1_RECT_F labelRc1 = D2D1::RectF(left + 30.0f, top + 38.0f, left + 110.0f, top + 58.0f);
            m_pRenderTarget->DrawText(L"D2D Rect", 8, m_pSmallFormat.Get(),
                labelRc1, m_pWhiteBrush);

            D2D1_RECT_F labelRc2 = D2D1::RectF(left + 150.0f, top + 38.0f, left + 250.0f, top + 58.0f);
            m_pRenderTarget->DrawText(L"D2D Rect", 8, m_pSmallFormat.Get(),
                labelRc2, m_pWhiteBrush);

            D2D1_RECT_F labelRc3 = D2D1::RectF(left + 290.0f, top + 38.0f, right - 25.0f, top + 58.0f);
            m_pRenderTarget->DrawText(L"D2D Rect", 8, m_pSmallFormat.Get(),
                labelRc3, m_pWhiteBrush);
        }

        // ------------------------------------------------------------------
        // D2D 椭圆 — 抗锯齿，边缘非常平滑
        // ------------------------------------------------------------------
        {
            float ellipseTop = top + 100.0f;

            D2D1_ELLIPSE yellowEllipse = D2D1::Ellipse(
                D2D1::Point2F(left + 100.0f, ellipseTop + 40.0f), 70.0f, 40.0f);
            m_pRenderTarget->FillEllipse(&yellowEllipse, m_pOrangeBrush);
            m_pRenderTarget->DrawEllipse(&yellowEllipse, m_pBlackBrush, 2.0f);

            D2D1_RECT_F labelRc = D2D1::RectF(left + 55.0f, ellipseTop + 30.0f,
                                                left + 145.0f, ellipseTop + 50.0f);
            m_pRenderTarget->DrawText(L"D2D Ellipse", 11, m_pSmallFormat.Get(),
                labelRc, m_pBlackBrush);

            ComPtr<ID2D1SolidColorBrush> pPurpleBrush;
            m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(0.61f, 0.15f, 0.69f, 1.0f), &pPurpleBrush);

            float ellipseRight = right - 30.0f;
            float ellipseLeft = left + 190.0f;
            float ecx = (ellipseLeft + ellipseRight) / 2.0f;
            float erx = (ellipseRight - ellipseLeft) / 2.0f;

            D2D1_ELLIPSE purpleEllipse = D2D1::Ellipse(
                D2D1::Point2F(ecx, ellipseTop + 40.0f), erx, 40.0f);
            m_pRenderTarget->FillEllipse(&purpleEllipse, pPurpleBrush.Get());
            m_pRenderTarget->DrawEllipse(&purpleEllipse, m_pBlackBrush, 2.0f);

            D2D1_RECT_F labelRc2 = D2D1::RectF(ecx - 50.0f, ellipseTop + 30.0f,
                                                 ecx + 50.0f, ellipseTop + 50.0f);
            m_pRenderTarget->DrawText(L"D2D Ellipse", 11, m_pSmallFormat.Get(),
                labelRc2, m_pWhiteBrush);
        }

        // ------------------------------------------------------------------
        // D2D 直线 — 抗锯齿平滑
        // ------------------------------------------------------------------
        {
            float lineTop = top + 200.0f;

            m_pRenderTarget->DrawLine(
                D2D1::Point2F(left + 20.0f, lineTop),
                D2D1::Point2F(right - 20.0f, lineTop),
                m_pBlackBrush, 3.0f);

            // D2D 直线比 GDI 更平滑
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(left + 20.0f, lineTop + 20.0f),
                D2D1::Point2F(right - 20.0f, lineTop + 20.0f),
                m_pRedBrush, 2.0f);

            D2D1_RECT_F labelRc = D2D1::RectF(left + 20.0f, lineTop + 30.0f,
                                                right - 20.0f, lineTop + 50.0f);
            m_pRenderTarget->DrawText(L"D2D Line (anti-aliased)", 24,
                m_pSmallFormat.Get(), labelRc, m_pBlackBrush);
        }

        // D2D 标签
        {
            float labelY = bottom - 50.0f;
            D2D1_RECT_F tagRc = D2D1::RectF(left + 20.0f, labelY, right - 20.0f, labelY + 20.0f);
            m_pRenderTarget->DrawText(L"D2D (anti-aliased)", 19,
                m_pLabelFormat.Get(), tagRc, m_pBlueBrush);

            D2D1_RECT_F noteRc = D2D1::RectF(left + 20.0f, labelY + 22.0f, right - 20.0f, labelY + 42.0f);
            m_pRenderTarget->DrawText(L"FillRect / DrawEllipse / DrawLine", 34,
                m_pSmallFormat.Get(), noteRc, m_pBlackBrush);
        }
    }

    // ------------------------------------------------------------------
    // 中央混合区域 — GDI + D2D 混合绘制
    //
    // 在同一个 BeginDraw/EndDraw 块中：
    // 1. 先用 D2D 画背景和圆形
    // 2. 用 GetDC 获取 HDC，用 GDI 画矩形框和文字
    // 3. ReleaseDC 后继续用 D2D 画标注
    // ------------------------------------------------------------------
    void DrawMixedRegion(float width, float height)
    {
        // 混合区域位于窗口底部中间
        float mixW = 280.0f;
        float mixH = 80.0f;
        float mixX = (width - mixW) / 2.0f;
        float mixY = height - mixH - 15.0f;

        // 1. D2D 绘制背景和圆形
        D2D1_RECT_F mixRect = D2D1::RectF(mixX, mixY, mixX + mixW, mixY + mixH);
        m_pRenderTarget->FillRectangle(&mixRect, m_pMixedBgBrush);

        // D2D 画圆形
        D2D1_ELLIPSE circle = D2D1::Ellipse(
            D2D1::Point2F(mixX + 50.0f, mixY + mixH / 2.0f), 25.0f, 25.0f);
        m_pRenderTarget->FillEllipse(&circle, m_pBlueBrush);
        m_pRenderTarget->DrawEllipse(&circle, m_pBlackBrush, 1.0f);

        D2D1_ELLIPSE circle2 = D2D1::Ellipse(
            D2D1::Point2F(mixX + mixW - 50.0f, mixY + mixH / 2.0f), 25.0f, 25.0f);
        m_pRenderTarget->FillEllipse(&circle2, m_pRedBrush);
        m_pRenderTarget->DrawEllipse(&circle2, m_pBlackBrush, 1.0f);

        // 2. GDI 绘制 — 在 D2D 内容之上画矩形框和文字
        if (m_pGdiInterop)
        {
            HDC hdc = nullptr;
            HRESULT hr = m_pGdiInterop->GetDC(
                D2D1_DC_INITIALIZE_MODE_COPY, &hdc);
            if (SUCCEEDED(hr) && hdc)
            {
                int savedState = SaveDC(hdc);

                // GDI 画虚线矩形框
                HPEN hDashPen = CreatePen(PS_DASH, 1, RGB(100, 100, 100));
                SelectObject(hdc, hDashPen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc,
                    static_cast<int>(mixX) + 2,
                    static_cast<int>(mixY) + 2,
                    static_cast<int>(mixX + mixW) - 2,
                    static_cast<int>(mixY + mixH) - 2);
                DeleteObject(hDashPen);

                // GDI 画中间文字
                SetTextColor(hdc, RGB(60, 60, 60));
                SetBkMode(hdc, TRANSPARENT);
                HFONT hFont = CreateFont(
                    12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
                SelectObject(hdc, hFont);
                const wchar_t* mixText = L"GDI + D2D Mixed";
                int textX = static_cast<int>(mixX + mixW / 2.0f - 60);
                int textY = static_cast<int>(mixY + mixH / 2.0f - 8);
                TextOut(hdc, textX, textY, mixText, static_cast<int>(wcslen(mixText)));
                DeleteObject(hFont);

                RestoreDC(hdc, savedState);
                m_pGdiInterop->ReleaseDC(nullptr);
            }
        }

        // 3. D2D 边框
        m_pRenderTarget->DrawRectangle(&mixRect, m_pBlackBrush, 1.5f);
    }
};

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ThrowIfFailed(hr, "CoInitializeEx 失败");

    D2DGdiInteropDemo app;
    if (!app.Create(hInstance, L"D2DGdiInteropDemoClass",
                    L"Direct2D GDI \x4E92\x64CD\x4F5C\x793A\x4F8B",   // "互操作示例"
                    700, 450))
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    int result = app.RunMessageLoop();

    CoUninitialize();
    return result;
}
