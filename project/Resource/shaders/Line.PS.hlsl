// Line.PS.hlsl

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

// ピクセルシェーダーの出力（画面に描画される色）
float4 main(VertexShaderOutput input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}