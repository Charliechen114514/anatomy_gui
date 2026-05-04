// 顶点着色器 — 最简单的直通着色器
// 将输入的位置和颜色原样传递给光栅化器

struct VS_INPUT
{
    float4 pos : POSITION;   // 顶点位置（物体空间）
    float4 col : COLOR;      // 顶点颜色
};

struct PS_INPUT
{
    float4 pos : SV_POSITION; // 裁剪空间位置（系统自动插值）
    float4 col : COLOR;       // 插值后的颜色
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = input.pos;   // 直接传递，不做变换
    output.col = input.col;   // 直接传递颜色
    return output;
}
