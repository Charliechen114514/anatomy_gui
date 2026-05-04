// alpha.hlsl - 透明混合着色器
// 通过常量缓冲区中的 Alpha 值控制整体透明度

cbuffer Constants : register(b0)
{
    matrix WorldViewProj;
    float Alpha;
};

struct VS_INPUT { float4 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(WorldViewProj, input.pos);
    output.col = input.col;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return float4(input.col.rgb, input.col.a * Alpha);
}
