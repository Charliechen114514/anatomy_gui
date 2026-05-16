/**
 * TextRenderer.hpp — GDI 文字渲染到 D3D11 纹理
 *
 * 原理：
 * 1. 创建一个内存位图（DIB Section），用 GDI DrawText 渲染文字
 * 2. 将位图像素数据上传到 D3D11 纹理
 * 3. 纹理的 SRV 被 SpriteBatch 当做普通精灵贴图使用
 *
 * 使用 GDI 而非 D2D，确保 MSVC 和 MinGW 都能编译。
 */

#pragma once

#include <d3d11.h>
#include <windows.h>
#include <string>

#include "ComHelper.hpp"

class TextRenderer
{
public:
    ~TextRenderer()
    {
        if (m_memDC)  { DeleteDC(m_memDC); m_memDC = nullptr; }
        if (m_bitmap) { DeleteObject(m_bitmap); m_bitmap = nullptr; }
        m_srv.Reset();
    }

    void Initialize(ID3D11Device* device, int textureWidth, int textureHeight)
    {
        m_device = device;
        m_width  = textureWidth;
        m_height = textureHeight;

        // 创建内存 DC 和 DIB Section
        HDC screenDC = GetDC(nullptr);
        m_memDC = CreateCompatibleDC(screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = textureWidth;
        bmi.bmiHeader.biHeight      = -(textureHeight);  // 自顶向下
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        m_bitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screenDC);

        m_pixelData = static_cast<uint8_t*>(bits);
        SelectObject(m_memDC, m_bitmap);

        // 创建 D3D11 纹理（BGRA 格式，与 GDI 位图一致）
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width            = textureWidth;
        texDesc.Height           = textureHeight;
        texDesc.MipLevels        = 1;
        texDesc.ArraySize        = 1;
        texDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage            = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        ThrowIfFailed(
            device->CreateTexture2D(&texDesc, nullptr, &m_texture),
            "TextRenderer: CreateTexture2D 失败");

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = texDesc.Format;
        srvDesc.ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        ThrowIfFailed(
            device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv),
            "TextRenderer: CreateShaderResourceView 失败");

        // 创建字体
        m_font = CreateFontW(
            20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        SelectObject(m_memDC, m_font);
        SetBkMode(m_memDC, TRANSPARENT);
    }

    // 渲染文字到内部纹理
    void RenderText(const wchar_t* text, float fontSize = 20.0f,
                    float r = 1.0f, float g = 1.0f, float b = 1.0f)
    {
        if (!m_memDC || !m_pixelData) return;

        // 如果字体大小变了，重新创建字体
        int pxSize = static_cast<int>(fontSize);
        if (pxSize != m_currentFontSize)
        {
            m_currentFontSize = pxSize;
            if (m_font) DeleteObject(m_font);
            m_font = CreateFontW(
                pxSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SelectObject(m_memDC, m_font);
        }

        // 设置文字颜色（GDI 用 BGR）
        COLORREF textColor = RGB(
            static_cast<BYTE>(b * 255),
            static_cast<BYTE>(g * 255),
            static_cast<BYTE>(r * 255));
        SetTextColor(m_memDC, textColor);

        // 用透明背景色填充（GDI 不支持透明，用黑色代替，着色器中用颜色叠加）
        RECT clearRect = { 0, 0, m_width, m_height };
        HBRUSH blackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(m_memDC, &clearRect, blackBrush);

        // 绘制文字
        RECT textRect = { 8, 0, m_width - 8, m_height };
        DrawTextW(m_memDC, text, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 将像素数据上传到 D3D11 纹理
        // GDI DIB Section 用的是 BGRA（与 DXGI_FORMAT_B8G8R8A8_UNORM 一致）
        // 但行间距可能是 4 字节对齐，需要逐行复制
        UINT rowPitch = m_width * 4;
        D3D11_MAPPED_SUBRESOURCE mapped = {};

        ID3D11DeviceContext* ctx = nullptr;
        m_device->GetImmediateContext(&ctx);

        HRESULT hr = ctx->Map(m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            for (int y = 0; y < m_height; ++y)
            {
                memcpy(
                    static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch,
                    m_pixelData + y * rowPitch,
                    rowPitch);
            }
            ctx->Unmap(m_texture.Get(), 0);
        }
        ctx->Release();
    }

    ID3D11ShaderResourceView* GetTextureSRV() const { return m_srv.Get(); }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    ID3D11Device*                    m_device = nullptr;
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    HDC       m_memDC = nullptr;
    HBITMAP   m_bitmap = nullptr;
    uint8_t*  m_pixelData = nullptr;
    HFONT     m_font = nullptr;

    int m_width  = 0;
    int m_height = 0;
    int m_currentFontSize = 0;
};
