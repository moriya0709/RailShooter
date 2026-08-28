// Line.PS.hlsl

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

struct Material
{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

// ピクセルシェーダーの出力（画面に描画される色）
float4 main(VertexShaderOutput input) : SV_TARGET
{
    return gMaterial.color;
}