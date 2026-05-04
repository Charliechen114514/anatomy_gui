// Simple vertex shader - pass through position and color
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

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return input.col;
}
