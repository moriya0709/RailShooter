#include "object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 prevWVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // ワールド座標を計算
    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    // 画面座標
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.worldPosition = worldPos.xyz;
    output.texcoord = input.texcoord;
    // 法線もワールド空間へ
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.World));

    // ==========================================
    // ★追加: 現在と過去のクリップ空間座標をPSに渡す
    // ==========================================
    output.currentClipPos = output.position; // 現在の位置（output.positionと同じ）
    output.prevClipPos = mul(input.position, gTransformationMatrix.prevWVP); // 1フレーム前の位置
    
    return output;
}