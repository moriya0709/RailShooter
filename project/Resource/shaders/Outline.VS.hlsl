#include "Outline.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

cbuffer OutlineParam : register(b1)
{
    float thickness;
    float32_t4 color;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 outlineNormal : NORMAL1;
};

VertexShaderOutput main(VertexShaderInput input)
{
  VertexShaderOutput output;

    // 1. ローカル空間でそのまま法線方向に押し出す
    float32_t4 localPos = input.position;
    localPos.xyz += input.outlineNormal * thickness;
    
    // 2. 押し出した座標に対して、一気にWVPをかける（これで画面上の座標になる）
    output.position = mul(localPos, gTransformationMatrix.WVP);

    return output;
}