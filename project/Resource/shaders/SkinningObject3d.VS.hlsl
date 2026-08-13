#include "SkinningObject3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 prevWVP;
};

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t2);

struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    float weightSum = input.weight.x + input.weight.y + input.weight.z + input.weight.w;

    if (weightSum < 1e-5f)
    {
        // ウェイトが無い頂点はスキン変換せずそのまま使う
        skinned.position = input.position;
        skinned.normal = input.normal;
        return skinned;
    }
    
    // 位置の変換
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f;
    
    // 法線の変換
    skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal);
      
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    float32_t4 localPosition = input.position;
    float32_t3 localNormal = input.normal;
    
    // ワールド座標を計算
    float4 worldPos = mul(localPosition, gTransformationMatrix.World);
    
    // 画面座標
    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.worldPosition = worldPos.xyz;
    output.texcoord = input.texcoord;
    
    // 法線もワールド空間へ
    output.normal = normalize(mul(localNormal, (float32_t3x3) gTransformationMatrix.World));

    // モーションブラー
    output.currentClipPos = output.position;
    output.prevClipPos = mul(localPosition, gTransformationMatrix.prevWVP);
    
    return output;
}