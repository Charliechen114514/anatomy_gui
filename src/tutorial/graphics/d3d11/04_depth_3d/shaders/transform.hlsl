// transform.hlsl - 深度缓冲与 3D 变换着色器
// 使用 WorldViewProj 矩阵将顶点从模型空间变换到裁剪空间

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
