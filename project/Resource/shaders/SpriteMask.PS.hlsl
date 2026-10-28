#include "Sprite.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float3 pad1;
    float4x4 uvTransform;
    float3 emissive;
    float pad2;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    const float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    const float alpha = gMaterial.color.a * gTexture.Sample(gSampler, uv).a;
    clip(alpha - 0.001f);
    // 文字の透明度をそのままマスク値にする。背景の透明領域は0のまま、
    // アンチエイリアス境界だけを自然に補間できる。
    return float4(alpha, alpha, alpha, alpha);
}
