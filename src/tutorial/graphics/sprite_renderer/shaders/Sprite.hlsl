// Sprite.hlsl — 2D 精灵渲染着色器
// 支持正交投影、纹理采样和顶点颜色调制

Texture2D    SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

cbuffer SpriteConstants : register(b0)
{
    matrix OrthoProjection;   // 正交投影矩阵（屏幕空间 → 裁剪空间）
};

struct VS_INPUT
{
    float2 pos   : POSITION;  // 屏幕空间位置（像素）
    float2 tex   : TEXCOORD;  // 纹理坐标 [0,1]
    float4 color : COLOR;     // 顶点颜色调制 (RGBA)
};

struct PS_INPUT
{
    float4 pos   : SV_POSITION;
    float2 tex   : TEXCOORD;
    float4 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos   = mul(OrthoProjection, float4(input.pos, 0.0, 1.0));
    output.tex   = input.tex;
    output.color = input.color;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return SpriteTexture.Sample(SpriteSampler, input.tex) * input.color;
}
