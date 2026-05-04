// 像素着色器 — 最简单的颜色输出
// 将光栅化器插值后的颜色直接输出到渲染目标

struct PS_INPUT
{
    float4 pos : SV_POSITION; // 像素位置（由光栅化器自动填充）
    float4 col : COLOR;       // 插值后的顶点颜色
};

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return input.col;         // 直接返回插值颜色
}
