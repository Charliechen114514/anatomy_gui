/**
 * TextureLoader.hpp — 纹理加载工具
 *
 * 提供：
 * 1. LoadFromFile      — 使用 WIC 从 PNG/JPG 文件加载纹理
 * 2. CreateCheckerboard — 程序化生成棋盘格纹理
 * 3. CreateSolidTexture — 程序化生成纯色纹理（白色，配合顶点颜色调色）
 *
 * 所有函数使用输出参数接收 ComPtr，避免自定义 ComPtr 的移动语义问题。
 */

#pragma once

#include <d3d11.h>
#include <wincodec.h>
#include <cstdint>
#include <cmath>
#include <vector>
#include <cstdlib>

#include "ComHelper.hpp"

class TextureLoader
{
public:
    // 使用 WIC 从图片文件加载纹理，创建 SRV
    static void LoadFromFile(
        ID3D11Device* device, const wchar_t* path,
        ComPtr<ID3D11ShaderResourceView>& outSRV)
    {
        ComPtr<IWICImagingFactory> wicFactory;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),
            reinterpret_cast<void**>(wicFactory.GetAddressOf()));
        ThrowIfFailed(hr, "CoCreateInstance(WICImagingFactory) 失败");

        ComPtr<IWICBitmapDecoder> decoder;
        hr = wicFactory->CreateDecoderFromFilename(
            path, nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder);
        ThrowIfFailed(hr, "CreateDecoderFromFilename 失败");

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        ThrowIfFailed(hr, "GetFrame 失败");

        UINT width = 0, height = 0;
        frame->GetSize(&width, &height);

        ComPtr<IWICFormatConverter> converter;
        hr = wicFactory->CreateFormatConverter(&converter);
        ThrowIfFailed(hr, "CreateFormatConverter 失败");

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr, 0.0, WICBitmapPaletteTypeCustom);
        ThrowIfFailed(hr, "FormatConverter Initialize 失败");

        std::vector<uint8_t> pixels(width * height * 4);
        hr = converter->CopyPixels(
            nullptr, width * 4,
            static_cast<UINT>(pixels.size()), pixels.data());
        ThrowIfFailed(hr, "CopyPixels 失败");

        CreateFromPixels(device, width, height, pixels.data(), outSRV);
    }

    // 程序化生成棋盘格纹理
    static void CreateCheckerboard(
        ID3D11Device* device, int width, int height,
        uint32_t color1, uint32_t color2, int cellSize,
        ComPtr<ID3D11ShaderResourceView>& outSRV)
    {
        std::vector<uint8_t> pixels(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int cx = x / cellSize;
                int cy = y / cellSize;
                uint32_t color = ((cx + cy) % 2 == 0) ? color1 : color2;
                int idx = (y * width + x) * 4;
                pixels[idx + 0] = (color >>  0) & 0xFF;
                pixels[idx + 1] = (color >>  8) & 0xFF;
                pixels[idx + 2] = (color >> 16) & 0xFF;
                pixels[idx + 3] = (color >> 24) & 0xFF;
            }
        }
        CreateFromPixels(device, width, height, pixels.data(), outSRV);
    }

    // 程序化生成纯色纹理（白色），配合顶点颜色调制实现不同颜色的精灵
    static void CreateSolidTexture(
        ID3D11Device* device, int size,
        ComPtr<ID3D11ShaderResourceView>& outSRV)
    {
        std::vector<uint8_t> pixels(size * size * 4, 0xFF);
        CreateFromPixels(device, size, size, pixels.data(), outSRV);
    }

private:
    static void SetPixel(uint8_t* data, int width, int x, int y, uint32_t color)
    {
        int idx = (y * width + x) * 4;
        data[idx + 0] = (color >>  0) & 0xFF;
        data[idx + 1] = (color >>  8) & 0xFF;
        data[idx + 2] = (color >> 16) & 0xFF;
        data[idx + 3] = (color >> 24) & 0xFF;
    }

    static void CreateFromPixels(
        ID3D11Device* device, int width, int height, const uint8_t* pixels,
        ComPtr<ID3D11ShaderResourceView>& outSRV)
    {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width            = width;
        texDesc.Height           = height;
        texDesc.MipLevels        = 1;
        texDesc.ArraySize        = 1;
        texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage            = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem     = pixels;
        initData.SysMemPitch = width * 4;

        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &texture);
        ThrowIfFailed(hr, "CreateTexture2D 失败");

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = texDesc.Format;
        srvDesc.ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, outSRV.GetAddressOf());
        ThrowIfFailed(hr, "CreateShaderResourceView 失败");
    }
};
