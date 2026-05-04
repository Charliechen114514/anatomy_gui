// 顶点着色器 — 直通着色器
// 与 01_hlsl_hello 相同，将位置和颜色原样传递

struct VS_INPUT
{
    float4 pos : POSITION;
    float4 col : COLOR;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = input.pos;
    output.col = input.col;
    return output;
}
