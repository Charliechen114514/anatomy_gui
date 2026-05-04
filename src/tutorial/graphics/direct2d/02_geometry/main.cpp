/**
 * 02_geometry - Direct2D 几何体示例
 *
 * 本程序演示了 Direct2D 几何体的创建和使用：
 * 1. CreateRectangleGeometry        — 矩形几何体
 * 2. CreateEllipseGeometry          — 椭圆几何体
 * 3. CreateRoundedRectangleGeometry — 圆角矩形几何体
 * 4. CreatePathGeometry             — 路径几何体（自定义形状）
 * 5. DrawGeometry                   — 描边绘制（含自定义虚线样式）
 * 6. FillGeometry                   — 填充绘制
 * 7. D2D1_STROKE_STYLE_PROPERTIES  — 描边样式（虚线模式）
 *
 * 参考: tutorial/native_win32/30_ProgrammingGUI_Graphics_Direct2D_Geometry.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <cmath>

#include "ComHelper.hpp"
#include "RenderWindow.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// D2DGeometryDemo — 使用 RenderWindow 模板
// ============================================================================

class D2DGeometryDemo : public RenderWindow<D2DGeometryDemo>
{
public:
    ~D2DGeometryDemo()
    {
        DiscardResources();
        m_pFactory.Reset();
    }

    // ------------------------------------------------------------------
    // OnCreate — 窗口创建时初始化 D2D 资源
    // ------------------------------------------------------------------
    void OnCreate()
    {
        // 创建 D2D 工厂
        ThrowIfFailed(
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory),
            "D2D1CreateFactory 失败");

        CreateResources();
    }

    // ------------------------------------------------------------------
    // OnResize — 窗口大小改变时调整渲染目标
    // ------------------------------------------------------------------
    void OnResize(int width, int height)
    {
        if (m_pRenderTarget)
        {
            m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        }
    }

    // ------------------------------------------------------------------
    // OnDestroy — 窗口销毁时清理资源
    // ------------------------------------------------------------------
    void OnDestroy()
    {
        DiscardResources();
        m_pFactory.Reset();
    }

    // ------------------------------------------------------------------
    // Render — 每帧渲染
    // ------------------------------------------------------------------
    void Render()
    {
        HRESULT hr = S_OK;

        // 确保渲染目标可用
        if (!m_pRenderTarget)
        {
            hr = CreateResources();
            if (FAILED(hr)) return;
        }

        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(D2D1::ColorF(0.94f, 0.94f, 0.94f, 1.0f));

        // 3 列 2 行网格布局
        float width  = static_cast<float>(m_width);
        float height = static_cast<float>(m_height);
        float padX = 8.0f, padY = 8.0f;
        float cellW = (width  - padX * 4.0f) / 3.0f;
        float cellH = (height - padY * 3.0f) / 2.0f;

        // 绘制六个几何体示例
        DrawRectangleGeo(  padX,             padY,              cellW, cellH);
        DrawEllipseGeo(    padX * 2 + cellW, padY,              cellW, cellH);
        DrawRoundedRectGeo(padX * 3 + cellW * 2, padY,          cellW, cellH);
        DrawPathGeoStar(   padX,             padY * 2 + cellH,  cellW, cellH);
        DrawStrokeStyleDemo(padX * 2 + cellW, padY * 2 + cellH, cellW, cellH);
        DrawGeometryCompare(padX * 3 + cellW * 2, padY * 2 + cellH, cellW, cellH);

        hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            DiscardResources();
        }
    }

private:
    ComPtr<ID2D1Factory>          m_pFactory;
    ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
    ComPtr<ID2D1SolidColorBrush>  m_pBlackBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pRedBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pBlueBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pGreenBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pOrangeBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pWhiteBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pPurpleBrush;
    ComPtr<ID2D1SolidColorBrush>  m_pGrayBrush;

    // 几何体资源
    ComPtr<ID2D1RectangleGeometry>       m_pRectGeo;
    ComPtr<ID2D1EllipseGeometry>         m_pEllipseGeo;
    ComPtr<ID2D1RoundedRectangleGeometry> m_pRoundedRectGeo;
    ComPtr<ID2D1PathGeometry>            m_pStarPathGeo;

    // 描边样式
    ComPtr<ID2D1StrokeStyle> m_pDashStyle;
    ComPtr<ID2D1StrokeStyle> m_pDotStyle;

    // ------------------------------------------------------------------
    // 创建所有 D2D 资源
    // ------------------------------------------------------------------
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

        // 创建画刷
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black), &m_pBlackBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.90f, 0.22f, 0.21f, 1.0f), &m_pRedBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.18f, 0.55f, 0.92f, 1.0f), &m_pBlueBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.30f, 0.69f, 0.31f, 1.0f), &m_pGreenBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Orange), &m_pOrangeBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White), &m_pWhiteBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.61f, 0.15f, 0.69f, 1.0f), &m_pPurpleBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f), &m_pGrayBrush);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建几何体
        // ------------------------------------------------------------------

        // 矩形几何体
        hr = m_pFactory->CreateRectangleGeometry(
            D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f),
            &m_pRectGeo);
        if (FAILED(hr)) return hr;

        // 椭圆几何体
        hr = m_pFactory->CreateEllipseGeometry(
            D2D1::Ellipse(D2D1::Point2F(0.0f, 0.0f), 60.0f, 40.0f),
            &m_pEllipseGeo);
        if (FAILED(hr)) return hr;

        // 圆角矩形几何体
        hr = m_pFactory->CreateRoundedRectangleGeometry(
            D2D1::RoundedRect(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f), 15.0f, 15.0f),
            &m_pRoundedRectGeo);
        if (FAILED(hr)) return hr;

        // 路径几何体 — 自定义星形（使用贝塞尔曲线）
        hr = CreateStarPathGeometry();
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建描边样式（虚线和点线）
        // ------------------------------------------------------------------

        // 虚线样式 (dash)
        float dashes[] = { 6.0f, 3.0f };
        D2D1_STROKE_STYLE_PROPERTIES dashProps = D2D1::StrokeStyleProperties();
        dashProps.dashStyle  = D2D1_DASH_STYLE_CUSTOM;
        dashProps.dashOffset = 0.0f;
        hr = m_pFactory->CreateStrokeStyle(dashProps, dashes, 2, &m_pDashStyle);
        if (FAILED(hr)) return hr;

        // 点线样式 (dot)
        float dots[] = { 1.0f, 3.0f };
        D2D1_STROKE_STYLE_PROPERTIES dotProps = D2D1::StrokeStyleProperties();
        dotProps.dashStyle  = D2D1_DASH_STYLE_CUSTOM;
        dotProps.dashOffset = 0.0f;
        hr = m_pFactory->CreateStrokeStyle(dotProps, dots, 2, &m_pDotStyle);
        if (FAILED(hr)) return hr;

        return S_OK;
    }

    /**
     * 创建星形路径几何体
     *
     * ID2D1PathGeometry + ID2D1GeometrySink 用于定义自定义形状。
     * 流程：
     * 1. CreatePathGeometry 创建空路径
     * 2. Open 获取 GeometrySink
     * 3. 用 BeginFigure / AddLine / AddBezier 等添加图元
     * 4. EndFigure / Close 完成路径
     */
    HRESULT CreateStarPathGeometry()
    {
        HRESULT hr = m_pFactory->CreatePathGeometry(&m_pStarPathGeo);
        if (FAILED(hr)) return hr;

        ComPtr<ID2D1GeometrySink> pSink;
        hr = m_pStarPathGeo->Open(&pSink);
        if (FAILED(hr)) return hr;

        // 五角星的 5 个外顶点和 5 个内顶点
        float outerR = 60.0f;
        float innerR = 25.0f;
        int numPoints = 5;

        // 计算起始点（顶部外顶点）
        float startAngle = static_cast<float>(-M_PI / 2.0);
        float x = outerR * cosf(startAngle);
        float y = outerR * sinf(startAngle);

        pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);

        // 添加所有线段（外 -> 内 -> 外 -> ...）
        for (int i = 0; i < numPoints; ++i)
        {
            // 内顶点
            float innerAngle = startAngle + (i * 2.0f + 1.0f) * 3.14159265f / numPoints;
            float ix = innerR * cosf(innerAngle);
            float iy = innerR * sinf(innerAngle);
            pSink->AddLine(D2D1::Point2F(ix, iy));

            // 下一个外顶点
            float outerAngle = startAngle + (i + 1) * 2.0f * 3.14159265f / numPoints;
            float ox = outerR * cosf(outerAngle);
            float oy = outerR * sinf(outerAngle);
            pSink->AddLine(D2D1::Point2F(ox, oy));
        }

        pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = pSink->Close();

        return hr;
    }

    void DiscardResources()
    {
        m_pRenderTarget.Reset();
        m_pBlackBrush.Reset();
        m_pRedBrush.Reset();
        m_pBlueBrush.Reset();
        m_pGreenBrush.Reset();
        m_pOrangeBrush.Reset();
        m_pWhiteBrush.Reset();
        m_pPurpleBrush.Reset();
        m_pGrayBrush.Reset();
        m_pRectGeo.Reset();
        m_pEllipseGeo.Reset();
        m_pRoundedRectGeo.Reset();
        m_pStarPathGeo.Reset();
        m_pDashStyle.Reset();
        m_pDotStyle.Reset();
    }

    // ------------------------------------------------------------------
    // 绘制面板背景和标题
    // ------------------------------------------------------------------
    void DrawPanel(float x, float y, float w, float h)
    {
        D2D1_RECT_F panelRect = D2D1::RectF(x, y, x + w, y + h);
        m_pRenderTarget->FillRectangle(&panelRect, m_pWhiteBrush);
        m_pRenderTarget->DrawRectangle(&panelRect, m_pGrayBrush, 1.0f);
    }

    // ------------------------------------------------------------------
    // 单元 1: 矩形几何体
    // ------------------------------------------------------------------
    void DrawRectangleGeo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        if (!m_pRectGeo) return;

        // 几何体定义在 (0,0,120,80) 空间中，需要用变换矩阵平移到正确位置
        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;

        // 保存当前变换
        D2D1_MATRIX_3X2_F oldTransform;
        m_pRenderTarget->GetTransform(&oldTransform);

        // 平移到面板中心，居中显示
        m_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Translation(cx - 60.0f, cy - 40.0f));

        // FillGeometry — 填充几何体内部
        m_pRenderTarget->FillGeometry(m_pRectGeo.Get(), m_pBlueBrush);
        // DrawGeometry — 描边绘制几何体轮廓
        m_pRenderTarget->DrawGeometry(m_pRectGeo.Get(), m_pBlackBrush, 2.0f);

        m_pRenderTarget->SetTransform(oldTransform);
    }

    // ------------------------------------------------------------------
    // 单元 2: 椭圆几何体
    // ------------------------------------------------------------------
    void DrawEllipseGeo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        if (!m_pEllipseGeo) return;

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;

        D2D1_MATRIX_3X2_F oldTransform;
        m_pRenderTarget->GetTransform(&oldTransform);

        m_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Translation(cx, cy));

        m_pRenderTarget->FillGeometry(m_pEllipseGeo.Get(), m_pRedBrush);
        m_pRenderTarget->DrawGeometry(m_pEllipseGeo.Get(), m_pBlackBrush, 2.0f);

        m_pRenderTarget->SetTransform(oldTransform);
    }

    // ------------------------------------------------------------------
    // 单元 3: 圆角矩形几何体
    // ------------------------------------------------------------------
    void DrawRoundedRectGeo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        if (!m_pRoundedRectGeo) return;

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;

        D2D1_MATRIX_3X2_F oldTransform;
        m_pRenderTarget->GetTransform(&oldTransform);

        m_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Translation(cx - 60.0f, cy - 40.0f));

        m_pRenderTarget->FillGeometry(m_pRoundedRectGeo.Get(), m_pGreenBrush);
        m_pRenderTarget->DrawGeometry(m_pRoundedRectGeo.Get(), m_pBlackBrush, 2.0f);

        m_pRenderTarget->SetTransform(oldTransform);
    }

    // ------------------------------------------------------------------
    // 单元 4: 路径几何体 — 自定义星形
    // ------------------------------------------------------------------
    void DrawPathGeoStar(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        if (!m_pStarPathGeo) return;

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;

        D2D1_MATRIX_3X2_F oldTransform;
        m_pRenderTarget->GetTransform(&oldTransform);

        // 平移 + 旋转
        m_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Rotation(0.0f, D2D1::Point2F(0.0f, 0.0f)) *
            D2D1::Matrix3x2F::Translation(cx, cy));

        m_pRenderTarget->FillGeometry(m_pStarPathGeo.Get(), m_pOrangeBrush);
        m_pRenderTarget->DrawGeometry(m_pStarPathGeo.Get(), m_pBlackBrush, 2.0f);

        m_pRenderTarget->SetTransform(oldTransform);
    }

    // ------------------------------------------------------------------
    // 单元 5: 描边样式演示（虚线 / 点线）
    // ------------------------------------------------------------------
    void DrawStrokeStyleDemo(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        float left   = x + 20.0f;
        float right  = x + w - 20.0f;
        float top    = y + 20.0f;
        float bottom = y + h - 20.0f;
        float midY   = (top + bottom) / 2.0f;

        // 实线
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(left, top + 15.0f),
            D2D1::Point2F(right, top + 15.0f),
            m_pBlackBrush, 2.0f);

        // 虚线 (使用自定义 StrokeStyle)
        if (m_pDashStyle)
        {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(left, midY),
                D2D1::Point2F(right, midY),
                m_pRedBrush, 2.0f, m_pDashStyle.Get());
        }

        // 点线 (使用自定义 StrokeStyle)
        if (m_pDotStyle)
        {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(left, bottom - 15.0f),
                D2D1::Point2F(right, bottom - 15.0f),
                m_pBlueBrush, 2.0f, m_pDotStyle.Get());
        }

        // 虚线描边的矩形
        if (m_pDashStyle)
        {
            D2D1_RECT_F dashRect = D2D1::RectF(left + 10.0f, top + 35.0f,
                                                 right - 10.0f, midY - 10.0f);
            m_pRenderTarget->DrawRectangle(&dashRect, m_pPurpleBrush, 2.0f, m_pDashStyle.Get());
        }
    }

    // ------------------------------------------------------------------
    // 单元 6: DrawGeometry vs FillGeometry 对比
    //
    // 左侧使用 DrawGeometry 仅描边（虚线样式），
    // 右侧使用 FillGeometry 填充后再描边。
    // ------------------------------------------------------------------
    void DrawGeometryCompare(float x, float y, float w, float h)
    {
        DrawPanel(x, y, w, h);

        float halfW = w / 2.0f;
        float rx = halfW / 2.0f - 25.0f;
        float ry = h / 2.0f - 35.0f;
        if (rx < 10.0f) rx = 10.0f;
        if (ry < 10.0f) ry = 10.0f;

        // 左侧：仅描边 (DrawGeometry) — 使用几何体对象
        {
            ComPtr<ID2D1EllipseGeometry> pStrokeGeo;
            m_pFactory->CreateEllipseGeometry(
                D2D1::Ellipse(
                    D2D1::Point2F(x + halfW / 2.0f, y + h / 2.0f),
                    rx, ry),
                &pStrokeGeo);

            if (pStrokeGeo)
            {
                // DrawGeometry — 仅描边，不填充
                m_pRenderTarget->DrawGeometry(
                    pStrokeGeo.Get(), m_pPurpleBrush, 3.0f, m_pDashStyle.Get());
            }
        }

        // 右侧：先填充再描边 (FillGeometry + DrawGeometry)
        {
            ComPtr<ID2D1EllipseGeometry> pFillGeo;
            m_pFactory->CreateEllipseGeometry(
                D2D1::Ellipse(
                    D2D1::Point2F(x + halfW + halfW / 2.0f, y + h / 2.0f),
                    rx, ry),
                &pFillGeo);

            if (pFillGeo)
            {
                // FillGeometry — 填充内部
                m_pRenderTarget->FillGeometry(pFillGeo.Get(), m_pBlueBrush);
                // DrawGeometry — 描边轮廓
                m_pRenderTarget->DrawGeometry(pFillGeo.Get(), m_pBlackBrush, 1.0f);
            }
        }
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

    // 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ThrowIfFailed(hr, "CoInitializeEx 失败");

    // 创建并运行渲染窗口
    D2DGeometryDemo app;
    if (!app.Create(hInstance, L"D2DGeometryDemoClass",
                    L"Direct2D \x51E0\x4F55\x4F53\x793A\x4F8B",   // "几何体示例"
                    700, 500))
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    int result = app.RunMessageLoop();

    CoUninitialize();
    return result;
}
