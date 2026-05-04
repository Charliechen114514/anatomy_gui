#pragma once

#include <d3d12.h>

// Some SDKs (MinGW, newer Windows SDK) lack CD3D12_* helper types and D3D12_DEFAULT
#ifndef CD3D12_RASTERIZER_DESC

#define D3D12_DEFAULT 0

struct CD3D12_RASTERIZER_DESC : D3D12_RASTERIZER_DESC {
    CD3D12_RASTERIZER_DESC(int) {
        FillMode = D3D12_FILL_MODE_SOLID;
        CullMode = D3D12_CULL_MODE_BACK;
        FrontCounterClockwise = FALSE;
        DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        DepthClipEnable = TRUE;
        MultisampleEnable = FALSE;
        AntialiasedLineEnable = FALSE;
        ForcedSampleCount = 0;
        ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
};

struct CD3D12_BLEND_DESC : D3D12_BLEND_DESC {
    CD3D12_BLEND_DESC(int) {
        AlphaToCoverageEnable = FALSE;
        IndependentBlendEnable = FALSE;
        for (UINT i = 0; i < 8; ++i) {
            RenderTarget[i].BlendEnable = FALSE;
            RenderTarget[i].LogicOpEnable = FALSE;
            RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
            RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
            RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
            RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
            RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
    }
};

struct CD3D12_DEPTH_STENCIL_DESC : D3D12_DEPTH_STENCIL_DESC {
    CD3D12_DEPTH_STENCIL_DESC(int) {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        BackFace = FrontFace;
    }
};

#endif // CD3D12_RASTERIZER_DESC
