#include "Skybox.hlsli"

// prevWVP はモーションブラー用に CPU から渡される前フレームの変換行列。
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 prevWVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // 深度を最遠面に固定し、Skybox を常にシーンジオメトリの背後へ描画する。
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.position.xyz;
    return output;
}
