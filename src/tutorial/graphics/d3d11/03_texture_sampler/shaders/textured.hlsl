Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

struct VS_INPUT
{
    float4 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = input.pos;
    output.tex = input.tex;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return txDiffuse.Sample(samLinear, input.tex);
}
