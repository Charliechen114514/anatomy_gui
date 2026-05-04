// 故意包含错误的像素着色器 — 用于演示编译错误调试
// 此着色器引用了未声明的变量 'undefinedVar'
// D3DCompileFromFile 会返回编译错误，可从 ID3DBlob 获取错误信息

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return undefinedVar;  // 错误: 未声明的标识符 'undefinedVar'
}
