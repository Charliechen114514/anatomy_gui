// 正确的像素着色器 — 用于编译调试演示的正常着色器

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return input.col;
}
