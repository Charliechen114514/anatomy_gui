// D3D12 根签名示例着色器
// 演示通过常量缓冲区 (cbuffer) 传递变换矩阵

cbuffer Constants : register(b0)
{
    matrix WorldViewProj;
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

float4 PSMain(PS_INPUT input) : SV_TARGET { return input.col; }
