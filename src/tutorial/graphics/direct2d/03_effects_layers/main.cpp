/**
 * 03_effects_layers - Direct2D 效果与图层示例
 *
 * 本程序演示了 Direct2D 的图层和效果功能：
 * 1. CreateLayer + PushLayer / PopLayer       — 图层管理
 * 2. 不透明蒙版层 (Opacity Mask Layer)         — 用画刷控制透明度
 * 3. 几何蒙版层 (Geometric Mask Layer)         — 裁剪到指定形状
 * 4. FillGeometry with opacity brush           — 渐变透明度填充
 * 5. ID2D1BitmapRenderTarget                  — 离屏渲染
 * 6. 三组重叠圆形展示不同图层效果
 *
 * 图层 (Layer) 允许将绘制操作分组，统一应用透明度/蒙版/变换。
 * PushLayer 开始一个图层，PopLayer 结束并应用效果。
 *
 * 参考: tutorial/native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <algorithm>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>

#include "ComHelper.hpp"
#include "RenderWindow.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// D2DEffectsDemo — 效果与图层演示
// ============================================================================

class D2DEffectsDemo : public RenderWindow<D2DEffectsDemo>
{
public:
    ~D2DEffectsDemo()
    {
        DiscardResources();
        m_pFactory.Reset();
    }

    void OnCreate()
    {
        ThrowIfFailed(
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory),
            "D2D1CreateFactory 失败");

        // 创建 DirectWrite 工厂（用于绘制标签文字）
        ThrowIfFailed(
            DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(m_pDWriteFactory.GetAddressOf())),
            "DWriteCreateFactory 失败");

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
        m_pDWriteFactory.Reset();
    }

    void Render()
    {
        if (!m_pRenderTarget)
        {
            if (FAILED(CreateResources())) return;
        }

        HRESULT hr = S_OK;
        m_pRenderTarget->BeginDraw();

        // 清除背景 — 深色背景让效果更明显
        m_pRenderTarget->Clear(D2D1::ColorF(0.12f, 0.12f, 0.14f, 1.0f));

        float width  = static_cast<float>(m_width);
        float height = static_cast<float>(m_height);

        // 分为上中下三个区域
        float panelH = (height - 40.0f) / 3.0f;
        float panelW = width - 40.0f;
        float startX = 20.0f;
        float startY = 20.0f;

        // 区域 1: 不透明蒙版层 (Opacity Mask)
        DrawOpacityMaskDemo(startX, startY, panelW, panelH);

        // 区域 2: 几何蒙版层 (Geometric Mask)
        DrawGeometricMaskDemo(startX, startY + panelH + 10.0f, panelW, panelH);

        // 区域 3: 离屏渲染 (BitmapRenderTarget)
        DrawBitmapRenderTargetDemo(startX, startY + (panelH + 10.0f) * 2.0f, panelW, panelH);

        hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            DiscardResources();
        }
    }

private:
    ComPtr<ID2D1Factory>          m_pFactory;
    ComPtr<IDWriteFactory>        m_pDWriteFactory;
    ComPtr<IDWriteTextFormat>     m_pTextFormat;
    ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;

    // 画刷
    ComPtr<ID2D1SolidColorBrush>  m_pWhiteBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pRedBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pBlueBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pGreenBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pYellowBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pBlackBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pDarkBgBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pLabelBrush;

    // 渐变画刷（用于不透明蒙版）
    ComPtr<ID2D1LinearGradientBrush> m_pOpacityGradientBrush;

    // 图层
    ComPtr<ID2D1Layer> m_pLayer;

    // 离屏渲染目标
    ComPtr<ID2D1BitmapRenderTarget> m_pBitmapTarget;
    ComPtr<ID2D1Bitmap>             m_pOffscreenBitmap;

    HRESULT CreateResources()
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        HRESULT hr = m_pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(
                m_hwnd,
                D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            &m_pRenderTarget);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建纯色画刷
        // ------------------------------------------------------------------
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White), &m_pWhiteBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.90f, 0.30f, 0.24f, 1.0f), &m_pRedBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.20f, 0.60f, 0.86f, 1.0f), &m_pBlueBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.30f, 0.69f, 0.31f, 1.0f), &m_pGreenBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Yellow), &m_pYellowBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black), &m_pBlackBrush);
        if (FAILED(hr)) return hr;

        // 深色背景画刷
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.18f, 0.18f, 0.20f, 1.0f), &m_pDarkBgBrush);
        if (FAILED(hr)) return hr;

        // 标签文字画刷
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &m_pLabelBrush);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建线性渐变画刷（用于不透明蒙版 — 从不透明到透明）
        // ------------------------------------------------------------------
        {
            D2D1_GRADIENT_STOP gradientStops[2];
            gradientStops[0].color    = D2D1::ColorF(D2D1::ColorF::White, 1.0f);  // 完全不透明
            gradientStops[0].position = 0.0f;
            gradientStops[1].color    = D2D1::ColorF(D2D1::ColorF::White, 0.0f);  // 完全透明
            gradientStops[1].position = 1.0f;

            ComPtr<ID2D1GradientStopCollection> pStopCollection;
            hr = m_pRenderTarget->CreateGradientStopCollection(
                gradientStops, 2, &pStopCollection);
            if (FAILED(hr)) return hr;

            hr = m_pRenderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0f, 0.0f),
                    D2D1::Point2F(1.0f, 0.0f)),
                pStopCollection.Get(),
                &m_pOpacityGradientBrush);
            if (FAILED(hr)) return hr;
        }

        // ------------------------------------------------------------------
        // 创建图层
        //
        // 图层对象可复用，PushLayer 时指定参数，PopLayer 时应用效果。
        // ------------------------------------------------------------------
        hr = m_pRenderTarget->CreateLayer(&m_pLayer);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建文字格式（用于标签）
        // ------------------------------------------------------------------
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us",
            &m_pTextFormat);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建离屏渲染目标并预渲染内容
        // ------------------------------------------------------------------
        hr = m_pRenderTarget->CreateCompatibleRenderTarget(
            D2D1::SizeF(200.0f, 150.0f), &m_pBitmapTarget);
        if (FAILED(hr)) return hr;

        RenderOffscreenContent();

        return S_OK;
    }

    /**
     * 在离屏渲染目标上预渲染内容
     *
     * ID2D1BitmapRenderTarget 允许在内存中绘制，
     * 然后通过 GetBitmap 获取 ID2D1Bitmap 用于主渲染。
     */
    void RenderOffscreenContent()
    {
        if (!m_pBitmapTarget) return;

        m_pBitmapTarget->BeginDraw();
        m_pBitmapTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        // 在离屏目标上绘制三个半透明重叠圆形
        ComPtr<ID2D1SolidColorBrush> pOffRed, pOffBlue, pOffGreen;
        m_pBitmapTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.90f, 0.30f, 0.24f, 0.7f), &pOffRed);
        m_pBitmapTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.20f, 0.60f, 0.86f, 0.7f), &pOffBlue);
        m_pBitmapTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.30f, 0.69f, 0.31f, 0.7f), &pOffGreen);

        float r = 45.0f;
        D2D1_ELLIPSE e1 = D2D1::Ellipse(D2D1::Point2F(55.0f,  50.0f), r, r);
        D2D1_ELLIPSE e2 = D2D1::Ellipse(D2D1::Point2F(105.0f, 50.0f), r, r);
        D2D1_ELLIPSE e3 = D2D1::Ellipse(D2D1::Point2F(80.0f,  90.0f), r, r);

        m_pBitmapTarget->FillEllipse(&e1, pOffRed.Get());
        m_pBitmapTarget->FillEllipse(&e2, pOffBlue.Get());
        m_pBitmapTarget->FillEllipse(&e3, pOffGreen.Get());

        m_pBitmapTarget->EndDraw();

        // 获取位图用于后续绘制
        m_pBitmapTarget->GetBitmap(&m_pOffscreenBitmap);
    }

    void DiscardResources()
    {
        m_pRenderTarget.Reset();
        m_pWhiteBrush.Reset();
        m_pRedBrush.Reset();
        m_pBlueBrush.Reset();
        m_pGreenBrush.Reset();
        m_pYellowBrush.Reset();
        m_pBlackBrush.Reset();
        m_pDarkBgBrush.Reset();
        m_pLabelBrush.Reset();
        m_pOpacityGradientBrush.Reset();
        m_pLayer.Reset();
        m_pBitmapTarget.Reset();
        m_pOffscreenBitmap.Reset();
        m_pTextFormat.Reset();
    }

    // ------------------------------------------------------------------
    // 绘制三个重叠圆形的辅助函数
    // ------------------------------------------------------------------
    void DrawThreeCircles(float cx, float cy, float r,
                          ID2D1SolidColorBrush* pBrush1,
                          ID2D1SolidColorBrush* pBrush2,
                          ID2D1SolidColorBrush* pBrush3)
    {
        float dx = r * 0.5f;
        float dy = r * 0.3f;

        D2D1_ELLIPSE e1 = D2D1::Ellipse(D2D1::Point2F(cx - dx, cy - dy), r, r);
        D2D1_ELLIPSE e2 = D2D1::Ellipse(D2D1::Point2F(cx + dx, cy - dy), r, r);
        D2D1_ELLIPSE e3 = D2D1::Ellipse(D2D1::Point2F(cx, cy + dy), r, r);

        m_pRenderTarget->FillEllipse(&e1, pBrush1);
        m_pRenderTarget->FillEllipse(&e2, pBrush2);
        m_pRenderTarget->FillEllipse(&e3, pBrush3);

        m_pRenderTarget->DrawEllipse(&e1, m_pBlackBrush, 1.0f);
        m_pRenderTarget->DrawEllipse(&e2, m_pBlackBrush, 1.0f);
        m_pRenderTarget->DrawEllipse(&e3, m_pBlackBrush, 1.0f);
    }

    // ------------------------------------------------------------------
    // 绘制面板背景
    // ------------------------------------------------------------------
    void DrawPanel(float x, float y, float w, float h)
    {
        D2D1_RECT_F panelRect = D2D1::RectF(x, y, x + w, y + h);
        m_pRenderTarget->FillRectangle(&panelRect, m_pDarkBgBrush);
        m_pRenderTarget->DrawRectangle(&panelRect, m_pWhiteBrush, 1.0f);
    }

    // ------------------------------------------------------------------
    // 绘制标签文字
    // ------------------------------------------------------------------
    void DrawLabel(float x, float y, const wchar_t* text)
    {
        if (!m_pTextFormat || !m_pLabelBrush) return;

        D2D1_RECT_F textRect = D2D1::RectF(x, y, x + 400.0f, y + 20.0f);
        m_pRenderTarget->DrawText(
            text,
            static_cast<UINT32>(wcslen(text)),
            m_pTextFormat.Get(),
            textRect,
            m_pLabelBrush);
    }

    // ------------------------------------------------------------------
    // 区域 1: 不透明蒙版层 (Opacity Mask)
    //
    // 使用 PushLayer 应用渐变透明蒙版：
    // 图层内所有绘制操作都会被蒙版画刷调制透明度。
    // 左侧完全不透明，右侧完全透明。
    // ------------------------------------------------------------------
    void DrawOpacityMaskDemo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;
        float r = std::min(w, h) / 3.5f;
        if (r < 20.0f) r = 20.0f;

        // 设置渐变画刷的起止点（从不透明渐变到透明）
        m_pOpacityGradientBrush->SetStartPoint(D2D1::Point2F(x + 20.0f, cy));
        m_pOpacityGradientBrush->SetEndPoint(D2D1::Point2F(x + w - 20.0f, cy));

        D2D1_RECT_F contentRect = D2D1::RectF(x, y, x + w, y + h);

        // PushLayer — 指定不透明蒙版画刷
        D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
            contentRect,
            nullptr,                                    // 无几何蒙版
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(),
            1.0f,                                       // 全局不透明度
            m_pOpacityGradientBrush.Get(),               // 不透明蒙版画刷
            D2D1_LAYER_OPTIONS_NONE);

        m_pRenderTarget->PushLayer(layerParams, m_pLayer.Get());

        // 在图层内绘制三个彩色圆
        DrawThreeCircles(cx, cy, r, m_pRedBrush, m_pBlueBrush, m_pGreenBrush);

        // PopLayer — 应用不透明蒙版效果
        m_pRenderTarget->PopLayer();

        DrawLabel(x + 10.0f, y + 5.0f, L"Opacity Mask Layer (left opaque, right transparent)");
    }

    // ------------------------------------------------------------------
    // 区域 2: 几何蒙版层 (Geometric Mask)
    //
    // 使用几何体作为蒙版，将绘制裁剪到指定形状内。
    // 彩色条纹只在椭圆区域内部可见。
    // ------------------------------------------------------------------
    void DrawGeometricMaskDemo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;

        // 创建椭圆蒙版
        float maskRx = std::min(w, h) * 0.4f;
        float maskRy = std::min(w, h) * 0.3f;

        ComPtr<ID2D1EllipseGeometry> pMaskGeo;
        m_pFactory->CreateEllipseGeometry(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), maskRx, maskRy),
            &pMaskGeo);

        if (pMaskGeo)
        {
            D2D1_RECT_F contentRect = D2D1::RectF(x, y, x + w, y + h);

            // PushLayer — 指定几何蒙版
            D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
                contentRect,
                pMaskGeo.Get(),                          // 几何蒙版 — 裁剪到此形状
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                1.0f,
                nullptr,                                 // 无不透明蒙版
                D2D1_LAYER_OPTIONS_NONE);

            m_pRenderTarget->PushLayer(layerParams, m_pLayer.Get());

            // 在图层内绘制彩虹条纹
            float stripeW = w / 8.0f;
            for (int i = 0; i < 8; ++i)
            {
                float hue = static_cast<float>(i) / 8.0f;
                float r = 0.5f + 0.5f * sinf(hue * 2.0f * 3.14159265f);
                float g = 0.5f + 0.5f * sinf(hue * 2.0f * 3.14159265f + 2.094f);
                float b = 0.5f + 0.5f * sinf(hue * 2.0f * 3.14159265f + 4.189f);

                ComPtr<ID2D1SolidColorBrush> pStripe;
                m_pRenderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(r, g, b, 1.0f), &pStripe);

                D2D1_RECT_F stripeRect = D2D1::RectF(
                    x + i * stripeW, y, x + (i + 1) * stripeW, y + h);
                m_pRenderTarget->FillRectangle(&stripeRect, pStripe.Get());
            }

            // PopLayer — 应用几何蒙版
            m_pRenderTarget->PopLayer();

            // 绘制蒙版轮廓
            D2D1_ELLIPSE maskOutline = D2D1::Ellipse(
                D2D1::Point2F(cx, cy), maskRx, maskRy);
            m_pRenderTarget->DrawEllipse(&maskOutline, m_pWhiteBrush, 1.5f);
        }

        DrawLabel(x + 10.0f, y + 5.0f, L"Geometric Mask Layer (clip to ellipse)");
    }

    // ------------------------------------------------------------------
    // 区域 3: 离屏渲染 (BitmapRenderTarget)
    //
    // 先在内存中绘制内容，再通过 DrawBitmap 绘制到主渲染目标。
    // 这种技术常用于缓存复杂内容、提高渲染性能。
    // ------------------------------------------------------------------
    void DrawBitmapRenderTargetDemo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        if (m_pOffscreenBitmap)
        {
            // 计算绘制位置（居中缩放）
            D2D1_SIZE_F bmpSize = m_pOffscreenBitmap->GetSize();
            float scale = std::min(
                (w - 40.0f) / bmpSize.width,
                (h - 40.0f) / bmpSize.height);
            if (scale < 0.1f) scale = 0.1f;

            float drawW = bmpSize.width * scale;
            float drawH = bmpSize.height * scale;
            float drawX = x + (w - drawW) / 2.0f;
            float drawY = y + (h - drawH) / 2.0f;

            D2D1_RECT_F drawRect = D2D1::RectF(drawX, drawY, drawX + drawW, drawY + drawH);

            // DrawBitmap — 将离屏位图绘制到主渲染目标
            m_pRenderTarget->DrawBitmap(
                m_pOffscreenBitmap.Get(),
                drawRect,
                0.9f,                                       // 不透明度
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);     // 插值模式

            // 边框
            m_pRenderTarget->DrawRectangle(&drawRect, m_pWhiteBrush, 1.0f);
        }

        DrawLabel(x + 10.0f, y + 5.0f, L"Off-screen BitmapRenderTarget (cached rendering)");
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

    D2DEffectsDemo app;
    if (!app.Create(hInstance, L"D2DEffectsDemoClass",
                    L"Direct2D \x6548\x679C\x4E0E\x56FE\x5C42\x793A\x4F8B",   // "效果与图层示例"
                    600, 500))
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    int result = app.RunMessageLoop();

    CoUninitialize();
    return result;
}
