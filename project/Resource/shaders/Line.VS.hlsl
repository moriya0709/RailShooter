// Line.VS.hlsl

// 頂点シェーダーへの入力（C++側の VertexData に対応）
// ※C++側で position しかセットしていなくても、HLSL側はこれだけで受け取れます
struct VertexShaderInput
{
    float4 position : POSITION;
};

// 頂点シェーダーからピクセルシェーダーへの出力
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

// C++側の TransformationMatrix 構造体に対応
struct TransformationMatrix
{
    matrix WVP;
    matrix World;
};

// 定数バッファ (WVP行列を受け取る)
// ※ registerの番号(b0やb1)は、LineCommon::CreateRootSignature の設定に合わせてください
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 頂点座標に WVP（World View Projection）行列を掛けて画面上の座標に変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    return output;
}