/**
 * SpriteBatch.hpp — D3D11 精灵批处理渲染器
 *
 * 核心设计：
 * 1. Begin / Draw / End 接口，模仿 XNA SpriteBatch 的使用模式
 * 2. 每帧动态生成四边形顶点，按纹理分组排序后合批绘制
 * 3. 使用动态顶点缓冲（D3D11_USAGE_DYNAMIC + MAP_WRITE_DISCARD）
 * 4. 正交投影实现屏幕空间 2D 渲染
 *
 * 用法：
 *   spriteBatch.Begin(context, width, height);
 *   spriteBatch.Draw(texture, x, y, ...);
 *   spriteBatch.End();
 */

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <cstring>
#include <cstdio>

#include "ComHelper.hpp"

// 每顶点 8 个 float：位置(2) + 纹理坐标(2) + 颜色(4) = 32 字节
struct SpriteVertex
{
    float x, y;       // 屏幕空间位置（像素）
    float u, v;       // 纹理坐标
    float r, g, b, a; // 颜色调制
};

// 精灵绘制参数
struct SpriteDrawParams
{
    float x, y;                          // 位置
    float width, height;                 // 输出尺寸（像素）
    float rotation;                      // 旋转角度（弧度）
    float originX, originY;              // 旋转/缩放中心
    float r, g, b, a;                    // 颜色调制
    float u0, v0, u1, v1;               // UV 子区域
    ID3D11ShaderResourceView* texture;   // 纹理 SRV
};

class SpriteBatch
{
public:
    void Initialize(ID3D11Device* device)
    {
        m_device = device;

        CompileShaders();
        CreateBlendState();
        CreateSamplerState();
        CreateConstantBuffer();
    }

    void Begin(ID3D11DeviceContext* context, float screenWidth, float screenHeight)
    {
        m_context = context;
        m_spriteQueue.clear();

        // 更新正交投影矩阵：左上角 (0,0)，右下角 (width, height)
        // 注意：第 3、4 参数交换（viewTop=0, viewBottom=h），产生负 Y 缩放
        // 以抵消 D3D11 视口变换的 Y 翻转，使得屏幕 Y=0 在顶部
        using namespace DirectX;
        XMMATRIX ortho = XMMatrixOrthographicOffCenterLH(
            0.0f, screenWidth, 0.0f, screenHeight, 0.0f, 1.0f);

        SpriteConstants cb;
        cb.OrthoProjection = XMMatrixTranspose(ortho);
        m_context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

        // 设置管线状态
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

        float blendFactor[4] = {};
        m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);

        // 禁用背面剔除（2D 精灵渲染需要双面绘制）
        if (!m_rasterizerState)
        {
            D3D11_RASTERIZER_DESC rsDesc = {};
            rsDesc.FillMode = D3D11_FILL_SOLID;
            rsDesc.CullMode = D3D11_CULL_NONE;
            rsDesc.ScissorEnable = FALSE;
            rsDesc.DepthClipEnable = TRUE;
            ThrowIfFailed(
            m_device->CreateRasterizerState(&rsDesc, &m_rasterizerState),
            "CreateRasterizerState 失败");
        }
        m_context->RSSetState(m_rasterizerState.Get());
    }

    void Draw(ID3D11ShaderResourceView* texture,
              float x, float y, float width, float height,
              float rotation = 0.0f,
              float originX = 0.0f, float originY = 0.0f,
              float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f,
              float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f)
    {
        SpriteDrawParams params = {};
        params.x = x;
        params.y = y;
        params.width = width;
        params.height = height;
        params.rotation = rotation;
        params.originX = originX;
        params.originY = originY;
        params.r = r; params.g = g; params.b = b; params.a = a;
        params.u0 = u0; params.v0 = v0; params.u1 = u1; params.v1 = v1;
        params.texture = texture;
        m_spriteQueue.push_back(params);
    }

    void End()
    {
        if (m_spriteQueue.empty()) return;

        // 生成顶点数据（按提交顺序，保持正确的前后层级）
        size_t vertexCount = m_spriteQueue.size() * VERTICES_PER_SPRITE;
        m_vertices.resize(vertexCount);

        size_t vertIdx = 0;
        for (const auto& sprite : m_spriteQueue)
        {
            EmitQuad(sprite, &m_vertices[vertIdx]);
            vertIdx += VERTICES_PER_SPRITE;
        }

        // 确保动态顶点缓冲足够大
        EnsureDynamicBuffer(vertexCount);

        // 上传顶点数据
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        m_context->Map(m_dynamicVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, m_vertices.data(), vertexCount * sizeof(SpriteVertex));
        m_context->Unmap(m_dynamicVB.Get(), 0);

        UINT stride = sizeof(SpriteVertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_dynamicVB.GetAddressOf(), &stride, &offset);

        // 按纹理分组发出 Draw 调用
        ID3D11ShaderResourceView* currentTexture = nullptr;
        size_t groupStart = 0;

        for (size_t i = 0; i < m_spriteQueue.size(); ++i)
        {
            if (m_spriteQueue[i].texture != currentTexture)
            {
                // 刷新前一个组
                if (currentTexture != nullptr)
                {
                    size_t count = i - groupStart;
                    m_context->PSSetShaderResources(0, 1, &currentTexture);
                    m_context->Draw(
                        static_cast<UINT>(count * VERTICES_PER_SPRITE),
                        static_cast<UINT>(groupStart * VERTICES_PER_SPRITE));
                }
                currentTexture = m_spriteQueue[i].texture;
                groupStart = i;
            }
        }

        // 刷新最后一个组
        if (currentTexture != nullptr)
        {
            size_t count = m_spriteQueue.size() - groupStart;
            m_context->PSSetShaderResources(0, 1, &currentTexture);
            m_context->Draw(
                static_cast<UINT>(count * VERTICES_PER_SPRITE),
                static_cast<UINT>(groupStart * VERTICES_PER_SPRITE));
        }
    }

private:
    static constexpr size_t VERTICES_PER_SPRITE = 6;
    static constexpr size_t INITIAL_MAX_SPRITES = 4096;

    struct SpriteConstants
    {
        DirectX::XMMATRIX OrthoProjection;
    };

    void CompileShaders()
    {
        const wchar_t* shaderPath = L"shaders/sprite_renderer/Sprite.hlsl";

        auto compile = [&](const char* entry, const char* model) -> ComPtr<ID3DBlob> {
            ComPtr<ID3DBlob> blob, errorBlob;
            HRESULT hr = D3DCompileFromFile(
                shaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry, model, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                &blob, &errorBlob);
            if (FAILED(hr))
            {
                if (errorBlob)
                    OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
                ThrowIfFailed(hr, "着色器编译失败");
            }
            return blob;
        };

        // 顶点着色器
        ComPtr<ID3DBlob> vsBlob = compile("VSMain", "vs_5_0");
        ThrowIfFailed(
            m_device->CreateVertexShader(
                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                nullptr, &m_vertexShader),
            "CreateVertexShader 失败");

        // 输入布局：position(2f) + texcoord(2f) + color(4f)
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        ThrowIfFailed(
            m_device->CreateInputLayout(
                layout, _countof(layout),
                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                &m_inputLayout),
            "CreateInputLayout 失败");

        // 像素着色器
        ComPtr<ID3DBlob> psBlob = compile("PSMain", "ps_5_0");
        ThrowIfFailed(
            m_device->CreatePixelShader(
                psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                nullptr, &m_pixelShader),
            "CreatePixelShader 失败");
    }

    void CreateBlendState()
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable    = TRUE;
        desc.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ThrowIfFailed(
            m_device->CreateBlendState(&desc, &m_blendState),
            "CreateBlendState 失败");
    }

    void CreateSamplerState()
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD         = 0.0f;
        desc.MaxLOD         = D3D11_FLOAT32_MAX;

        ThrowIfFailed(
            m_device->CreateSamplerState(&desc, &m_samplerState),
            "CreateSamplerState 失败");
    }

    void CreateConstantBuffer()
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth      = sizeof(SpriteConstants);
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

        ThrowIfFailed(
            m_device->CreateBuffer(&desc, nullptr, &m_constantBuffer),
            "CreateBuffer(常量缓冲) 失败");
    }

    void EnsureDynamicBuffer(size_t vertexCount)
    {
        size_t neededBytes = vertexCount * sizeof(SpriteVertex);
        if (m_dynamicVB && m_dynamicVBCapacity >= neededBytes)
            return;

        // 至少预留 INITIAL_MAX_SPRITES 个精灵的空间
        size_t maxVertices = (std::max)(vertexCount, INITIAL_MAX_SPRITES * VERTICES_PER_SPRITE);
        m_dynamicVBCapacity = maxVertices * sizeof(SpriteVertex);

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth      = static_cast<UINT>(m_dynamicVBCapacity);
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        ThrowIfFailed(
            m_device->CreateBuffer(&desc, nullptr, &m_dynamicVB),
            "CreateBuffer(动态顶点缓冲) 失败");
    }

    // 将一个精灵的四边形写入顶点数组（6 个顶点 = 2 个三角形）
    void EmitQuad(const SpriteDrawParams& p, SpriteVertex* verts)
    {
        float cosR = cosf(p.rotation);
        float sinR = sinf(p.rotation);

        // 四个角的局部坐标（相对于原点）
        float corners[4][2] = {
            { -p.originX,             -p.originY },            // 左上
            { p.width - p.originX,    -p.originY },            // 右上
            { p.width - p.originX,    p.height - p.originY },  // 右下
            { -p.originX,             p.height - p.originY },  // 左下
        };

        float screenCorners[4][2];
        for (int i = 0; i < 4; ++i)
        {
            screenCorners[i][0] = corners[i][0] * cosR - corners[i][1] * sinR + p.x;
            screenCorners[i][1] = corners[i][0] * sinR + corners[i][1] * cosR + p.y;
        }

        // UV 坐标
        float uv[4][2] = {
            { p.u0, p.v0 },
            { p.u1, p.v0 },
            { p.u1, p.v1 },
            { p.u0, p.v1 },
        };

        // 颜色
        float col[4] = { p.r, p.g, p.b, p.a };

        // 三角形 1: 0-1-2，三角形 2: 0-3-2（保证两个三角形都是 CW 绕序）
        int indices[6] = { 0, 1, 2, 0, 3, 2 };
        for (int i = 0; i < 6; ++i)
        {
            int ci = indices[i];
            verts[i].x = screenCorners[ci][0];
            verts[i].y = screenCorners[ci][1];
            verts[i].u = uv[ci][0];
            verts[i].v = uv[ci][1];
            verts[i].r = col[0];
            verts[i].g = col[1];
            verts[i].b = col[2];
            verts[i].a = col[3];
        }
    }

    ID3D11Device*        m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    ComPtr<ID3D11VertexShader>  m_vertexShader;
    ComPtr<ID3D11PixelShader>   m_pixelShader;
    ComPtr<ID3D11InputLayout>   m_inputLayout;
    ComPtr<ID3D11Buffer>        m_constantBuffer;
    ComPtr<ID3D11Buffer>        m_dynamicVB;
    ComPtr<ID3D11BlendState>    m_blendState;
    ComPtr<ID3D11SamplerState>  m_samplerState;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;

    std::vector<SpriteVertex>     m_vertices;
    std::vector<SpriteDrawParams> m_spriteQueue;
    size_t m_dynamicVBCapacity = 0;
};
