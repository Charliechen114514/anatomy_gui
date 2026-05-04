// Constant Buffer 着色器 — 演示如何通过 cbuffer 从 CPU 向 GPU 传递数据
// 使用常量缓冲区传递时间值，在像素着色器中计算动画颜色

cbuffer TimeColorBuffer : register(b0)
{
    float4 TimeColor;     // 动画颜色（本示例未使用，保留扩展）
    float  TimeValue;     // 经过的秒数（由 CPU 每帧更新）
    float3 Padding;       // 填充至 16 字节对齐
};

struct VS_INPUT
{
    float4 pos : POSITION;   // 顶点位置
};

struct PS_INPUT
{
    float4 pos : SV_POSITION; // 裁剪空间位置
};

// 顶点着色器 — 直通变换
PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = input.pos;
    return output;
}

// 像素着色器 — 基于时间值计算动画颜色
// 使用 sin 函数在 R/G/B 三个通道上产生相位差，实现彩虹色循环
float4 PSMain(PS_INPUT input) : SV_TARGET
{
    // 使用 sin 函数生成 [0, 1] 范围的颜色分量
    // 三个通道的相位差约为 2π/3 ≈ 2.094，形成三色循环
    float r = sin(TimeValue) * 0.5 + 0.5;
    float g = sin(TimeValue + 2.094) * 0.5 + 0.5;
    float b = sin(TimeValue + 4.189) * 0.5 + 0.5;
    return float4(r, g, b, 1.0);
}
