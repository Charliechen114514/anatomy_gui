/**
 * 04_directwrite - DirectWrite 文字排版示例
 *
 * 本程序演示了 DirectWrite 文字排版功能：
 * 1. DWriteCreateFactory              — 创建 DirectWrite 工厂
 * 2. CreateTextFormat                 — 文字格式（字体、大小、粗细、风格）
 * 3. CreateTextLayout                 — 多行文本布局（含格式化范围）
 * 4. DrawTextLayout                   — 使用 D2D 渲染目标绘制
 * 5. SetDrawingEffect                 — 为文本范围设置绘制效果（颜色等）
 * 6. TrimSign                         — 文本溢出标记（省略号）
 * 7. 文字换行 (Word Wrapping)
 *
 * 显示三个文本块：
 * - 标题：大号粗体
 * - 段落：带混合颜色的多行文本
 * - 等宽代码块：代码风格的文本
 *
 * 参考: tutorial/native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include "ComHelper.hpp"
#include "RenderWindow.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// DWriteDemo — DirectWrite 文字排版演示
// ============================================================================

class DWriteDemo : public RenderWindow<DWriteDemo>
{
public:
    ~DWriteDemo()
    {
        DiscardResources();
        m_pD2DFactory.Reset();
        m_pDWriteFactory.Reset();
    }

    void OnCreate()
    {
        ThrowIfFailed(
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory),
            "D2D1CreateFactory 失败");

        // 创建 DirectWrite 工厂
        ThrowIfFailed(
            DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,         // 共享工厂（系统字体缓存）
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
        m_pD2DFactory.Reset();
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

        // 清除背景 — 白色
        m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        float width  = static_cast<float>(m_width);
        float height = static_cast<float>(m_height);

        float padding = 30.0f;

        // ------------------------------------------------------------------
        // 区域 1: 标题文字
        //
        // 使用 CreateTextFormat 创建文字格式：
        // - 字体族: "微软雅黑" (Microsoft YaHei)
        // - 粗细: Bold
        // - 字号: 28pt
        // ------------------------------------------------------------------
        {
            D2D1_RECT_F titleRect = D2D1::RectF(
                padding, padding,
                width - padding, padding + 50.0f);

            m_pRenderTarget->DrawText(
                L"DirectWrite \x6587\x5B57\x6392\x7248\x793A\x4F8B",   // "文字排版示例"
                21,
                m_pTitleFormat.Get(),
                titleRect,
                m_pDarkBrush);

            // 副标题
            D2D1_RECT_F subtitleRect = D2D1::RectF(
                padding, padding + 50.0f,
                width - padding, padding + 75.0f);

            m_pRenderTarget->DrawText(
                L"Using DirectWrite + Direct2D for high-quality text rendering",
                60,
                m_pSubTitleFormat.Get(),
                subtitleRect,
                m_pGrayBrush);
        }

        // 分隔线
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(padding, padding + 85.0f),
            D2D1::Point2F(width - padding, padding + 85.0f),
            m_pLightGrayBrush, 1.0f);

        // ------------------------------------------------------------------
        // 区域 2: 混合颜色段落 (使用 IDWriteTextLayout)
        //
        // CreateTextLayout 允许为不同的文本范围设置不同格式：
        // - SetDrawingEffect — 设置绘制效果（如颜色画刷）
        // - SetFontWeight    — 设置字体粗细
        // - SetFontSize      — 设置字号
        // ------------------------------------------------------------------
        {
            float blockY = padding + 100.0f;
            float blockH = 130.0f;

            // 段落文本内容
            const wchar_t* paragraphText =
                L"DirectWrite \x63D0\x4F9B"           // "提供"
                L"\x9AD8\x8D28\x91CF\x7684"           // "高质量的"
                L" \x6587\x5B57\x6E32\x67D3"           // " 文字渲染"
                L" \x80FD\x529B\x3002"                 // " 能力。"
                L" \x5B83\x652F\x6301"                 // " 它支持"
                L" \x591A\x79CD\x5B57\x4F53\x683C\x5F0F"  // " 多种字体格式"
                L"\xFF0C\x5305\x62EC"                  // "，包括"
                L" TrueType\x3001OpenType"              // " TrueType、OpenType"
                L" \x548C CFF\x3002"                    // " 和 CFF。"
                L" \x901A\x8FC7 TextLayout\xFF0C"      // " 通过 TextLayout，"
                L" \x60A8\x53EF\x4EE5\x4E3A\x4E0D\x540C\x8303\x56F4\x7684\x6587\x5B57"
                L" \x8BBE\x7F6E\x4E0D\x540C\x7684\x683C\x5F0F\x548C\x989C\x8272\x3002";
                // " 您可以为不同范围的文字设置不同的格式和颜色。"

            UINT32 textLen = static_cast<UINT32>(wcslen(paragraphText));

            // 创建 TextLayout
            ComPtr<IDWriteTextLayout> pParagraphLayout;
            float layoutWidth = width - padding * 2.0f;
            ThrowIfFailed(m_pDWriteFactory->CreateTextLayout(
                paragraphText, textLen,
                m_pBodyFormat.Get(),
                layoutWidth,           // 最大宽度
                130.0f,                // 最大高度
                &pParagraphLayout));

            // 为文本范围设置不同的绘制效果（颜色）
            // "DirectWrite" — 红色
            DWRITE_TEXT_RANGE range1 = { 0, 12 };   // "DirectWrite "
            pParagraphLayout->SetDrawingEffect(m_pRedBrush.Get(), range1);

            // "高质量的" — 蓝色加粗
            DWRITE_TEXT_RANGE range2 = { 12, 5 };
            pParagraphLayout->SetDrawingEffect(m_pBlueBrush.Get(), range2);
            pParagraphLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range2);

            // "TrueType" — 绿色
            DWRITE_TEXT_RANGE range3;
            // 查找 "TrueType" 位置
            const wchar_t* trueTypePos = wcsstr(paragraphText, L"TrueType");
            if (trueTypePos)
            {
                range3.startPosition = static_cast<UINT32>(trueTypePos - paragraphText);
                range3.length = 8;
                pParagraphLayout->SetDrawingEffect(m_pGreenBrush.Get(), range3);
            }

            // "OpenType" — 橙色
            const wchar_t* openTypePos = wcsstr(paragraphText, L"OpenType");
            if (openTypePos)
            {
                DWRITE_TEXT_RANGE range4;
                range4.startPosition = static_cast<UINT32>(openTypePos - paragraphText);
                range4.length = 8;
                pParagraphLayout->SetDrawingEffect(m_pOrangeBrush.Get(), range4);
            }

            // "TextLayout" — 紫色
            const wchar_t* textLayoutPos = wcsstr(paragraphText, L"TextLayout");
            if (textLayoutPos)
            {
                DWRITE_TEXT_RANGE range5;
                range5.startPosition = static_cast<UINT32>(textLayoutPos - paragraphText);
                range5.length = 10;
                pParagraphLayout->SetDrawingEffect(m_pPurpleBrush.Get(), range5);
            }

            // 设置自动换行
            pParagraphLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

            // 使用 DrawTextLayout 绘制（而非 DrawText）
            D2D1_POINT_2F origin = D2D1::Point2F(padding, blockY);
            m_pRenderTarget->DrawTextLayout(
                origin,
                pParagraphLayout.Get(),
                m_pDarkBrush,                          // 默认文字颜色
                D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // 分隔线
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(padding, padding + 235.0f),
            D2D1::Point2F(width - padding, padding + 235.0f),
            m_pLightGrayBrush, 1.0f);

        // ------------------------------------------------------------------
        // 区域 3: 等宽代码块 + TrimSign（省略号）
        //
        // 使用等宽字体模拟代码显示：
        // - 背景色区分代码区域
        // - TrimSign 处理溢出文本
        // ------------------------------------------------------------------
        {
            float codeX = padding;
            float codeY = padding + 250.0f;
            float codeW = width - padding * 2.0f;
            float codeH = 180.0f;

            // 代码块背景
            D2D1_RECT_F codeBg = D2D1::RectF(codeX, codeY, codeX + codeW, codeY + codeH);
            m_pRenderTarget->FillRectangle(&codeBg, m_pCodeBgBrush);
            m_pRenderTarget->DrawRectangle(&codeBg, m_pLightGrayBrush, 1.0f);

            // 代码文本
            const wchar_t* codeText =
                L"// Create DirectWrite factory\n"
                L"DWriteCreateFactory(\n"
                L"    DWRITE_FACTORY_TYPE_SHARED,\n"
                L"    __uuidof(IDWriteFactory),\n"
                L"    &pFactory);\n"
                L"\n"
                L"// Create text format\n"
                L"pFactory->CreateTextFormat(\n"
                L"    L\"Consolas\", nullptr,\n"
                L"    DWRITE_FONT_WEIGHT_NORMAL,\n"
                L"    DWRITE_FONT_STYLE_NORMAL,\n"
                L"    DWRITE_FONT_STRETCH_NORMAL,\n"
                L"    14.0f, L\"en-us\",\n"
                L"    &pTextFormat);";

            UINT32 codeLen = static_cast<UINT32>(wcslen(codeText));

            ComPtr<IDWriteTextLayout> pCodeLayout;
            ThrowIfFailed(m_pDWriteFactory->CreateTextLayout(
                codeText, codeLen,
                m_pCodeFormat.Get(),
                codeW - 20.0f,
                codeH - 20.0f,
                &pCodeLayout));

            // 设置代码文本不换行（显示截断）
            pCodeLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            // 设置 TrimSign（文本溢出时显示省略号）
            DWRITE_TRIMMING trimming = {};
            trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            ComPtr<IDWriteInlineObject> pTrimSign;
            m_pDWriteFactory->CreateEllipsisTrimmingSign(
                m_pCodeFormat.Get(), &pTrimSign);
            pCodeLayout->SetTrimming(&trimming, pTrimSign.Get());

            // 绘制代码文本
            D2D1_POINT_2F codeOrigin = D2D1::Point2F(codeX + 10.0f, codeY + 10.0f);
            m_pRenderTarget->DrawTextLayout(
                codeOrigin,
                pCodeLayout.Get(),
                m_pCodeBrush,
                D2D1_DRAW_TEXT_OPTIONS_NONE);

            // 代码块标签
            D2D1_RECT_F labelRect = D2D1::RectF(codeX + 10.0f, codeY - 18.0f,
                                                  codeX + 100.0f, codeY);
            m_pRenderTarget->DrawText(
                L"CODE", 4,
                m_pLabelFormat.Get(),
                labelRect,
                m_pGrayBrush);
        }

        // ------------------------------------------------------------------
        // 底部: 字体信息展示
        // ------------------------------------------------------------------
        {
            float infoY = height - 60.0f;
            D2D1_RECT_F infoRect = D2D1::RectF(padding, infoY, width - padding, infoY + 40.0f);

            m_pRenderTarget->DrawText(
                L"Font: \x5FAE\x8F6F\x96C5\x9ED1 (Microsoft YaHei) | Consolas | "  // "微软雅黑"
                L"DirectWrite + Direct2D Rendering",
                80,
                m_pSubTitleFormat.Get(),
                infoRect,
                m_pGrayBrush);
        }

        hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            DiscardResources();
        }
    }

private:
    ComPtr<ID2D1Factory>          m_pD2DFactory;
    ComPtr<IDWriteFactory>        m_pDWriteFactory;
    ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;

    // DirectWrite 文字格式
    ComPtr<IDWriteTextFormat>     m_pTitleFormat;      // 标题格式
    ComPtr<IDWriteTextFormat>     m_pSubTitleFormat;   // 副标题格式
    ComPtr<IDWriteTextFormat>     m_pBodyFormat;       // 正文格式
    ComPtr<IDWriteTextFormat>     m_pCodeFormat;       // 代码格式（等宽）
    ComPtr<IDWriteTextFormat>     m_pLabelFormat;      // 标签格式

    // 画刷
    ComPtr<ID2D1SolidColorBrush>  m_pDarkBrush;        // 深色文字
    ComPtr<ID2D1SolidColorBrush>  m_pGrayBrush;        // 灰色文字
    ComPtr<ID2D1SolidColorBrush>  m_pLightGrayBrush;   // 浅灰色（分隔线）
    ComPtr<ID2D1SolidColorBrush>  m_pRedBrush;         // 红色
    ComPtr<ID2D1SolidColorBrush>  m_pBlueBrush;        // 蓝色
    ComPtr<ID2D1SolidColorBrush>  m_pGreenBrush;       // 绿色
    ComPtr<ID2D1SolidColorBrush>  m_pOrangeBrush;      // 橙色
    ComPtr<ID2D1SolidColorBrush>  m_pPurpleBrush;      // 紫色
    ComPtr<ID2D1SolidColorBrush>  m_pCodeBrush;        // 代码文字颜色
    ComPtr<ID2D1SolidColorBrush>  m_pCodeBgBrush;      // 代码背景色

    HRESULT CreateResources()
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(
                m_hwnd,
                D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            &m_pRenderTarget);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建画刷
        // ------------------------------------------------------------------
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.13f, 0.13f, 0.13f, 1.0f), &m_pDarkBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.45f, 0.45f, 0.45f, 1.0f), &m_pGrayBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f), &m_pLightGrayBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.85f, 0.17f, 0.16f, 1.0f), &m_pRedBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.13f, 0.42f, 0.78f, 1.0f), &m_pBlueBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.10f, 0.58f, 0.20f, 1.0f), &m_pGreenBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.90f, 0.49f, 0.05f, 1.0f), &m_pOrangeBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.40f, 0.23f, 0.71f, 1.0f), &m_pPurpleBrush);
        if (FAILED(hr)) return hr;

        // 代码颜色
        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.87f, 0.92f, 0.68f, 1.0f), &m_pCodeBrush);
        if (FAILED(hr)) return hr;

        hr = m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.15f, 0.16f, 0.18f, 1.0f), &m_pCodeBgBrush);
        if (FAILED(hr)) return hr;

        // ------------------------------------------------------------------
        // 创建 DirectWrite 文字格式
        //
        // CreateTextFormat 参数:
        //   fontFamilyName — 字体族名（如 "微软雅黑"、"Consolas"）
        //   fontCollection — 字体集合（nullptr = 系统字体）
        //   fontWeight     — 字体粗细（如 Bold, Normal, Light）
        //   fontStyle      — 字体风格（Normal, Italic, Oblique）
        //   fontStretch    — 字体拉伸（Normal, Condensed, Expanded）
        //   fontSize       — 字号（DIP 单位）
        //   localeName     — 区域设置（如 "zh-cn", "en-us"）
        // ------------------------------------------------------------------

        // 标题格式 — 微软雅黑 Bold 28pt
        hr = m_pDWriteFactory->CreateTextFormat(
            L"\x5FAE\x8F6F\x96C5\x9ED1",              // "微软雅黑"
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,                  // 粗体
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            28.0f,                                    // 字号
            L"zh-cn",
            &m_pTitleFormat);
        if (FAILED(hr)) return hr;
        m_pTitleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_pTitleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        // 副标题格式 — Consolas Normal 12pt
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"en-us",
            &m_pSubTitleFormat);
        if (FAILED(hr)) return hr;

        // 正文格式 — 微软雅黑 Normal 15pt
        hr = m_pDWriteFactory->CreateTextFormat(
            L"\x5FAE\x8F6F\x96C5\x9ED1",              // "微软雅黑"
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            15.0f,
            L"zh-cn",
            &m_pBodyFormat);
        if (FAILED(hr)) return hr;
        m_pBodyFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

        // 代码格式 — Consolas Normal 13pt
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f,
            L"en-us",
            &m_pCodeFormat);
        if (FAILED(hr)) return hr;
        m_pCodeFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        // 标签格式
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f,
            L"en-us",
            &m_pLabelFormat);
        if (FAILED(hr)) return hr;

        return S_OK;
    }

    void DiscardResources()
    {
        m_pRenderTarget.Reset();
        m_pDarkBrush.Reset();
        m_pGrayBrush.Reset();
        m_pLightGrayBrush.Reset();
        m_pRedBrush.Reset();
        m_pBlueBrush.Reset();
        m_pGreenBrush.Reset();
        m_pOrangeBrush.Reset();
        m_pPurpleBrush.Reset();
        m_pCodeBrush.Reset();
        m_pCodeBgBrush.Reset();
        m_pTitleFormat.Reset();
        m_pSubTitleFormat.Reset();
        m_pBodyFormat.Reset();
        m_pCodeFormat.Reset();
        m_pLabelFormat.Reset();
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

    DWriteDemo app;
    if (!app.Create(hInstance, L"DWriteDemoClass",
                    L"DirectWrite \x6587\x5B57\x6392\x7248\x793A\x4F8B",   // "文字排版示例"
                    700, 550))
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    int result = app.RunMessageLoop();

    CoUninitialize();
    return result;
}
