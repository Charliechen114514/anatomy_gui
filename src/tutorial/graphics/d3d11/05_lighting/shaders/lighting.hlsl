// lighting.hlsl - 方向光照着色器
// 在顶点着色器中计算漫反射 + 环境光光照

cbuffer Constants : register(b0)
{
    matrix World;
    matrix WorldViewProj;
    float3 LightDir;
    float Padding;
};

struct VS_INPUT { float4 pos : POSITION; float3 normal : NORMAL; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(WorldViewProj, input.pos);
    float3 worldNormal = normalize(mul((float3x3)World, input.normal));
    float diffuse = max(dot(worldNormal, -LightDir), 0.0f);
    float ambient = 0.2f;
    output.col = float4(input.col.rgb * (ambient + diffuse), input.col.a);
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET { return input.col; }
