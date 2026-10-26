// Particle.VS.hlsl
#include "Particle.hlsli"

cbuffer CameraData : register(b0)
{
    matrix viewProj;
    matrix billboardMatrix; // CPUの Inverse(view) を渡します
};

StructuredBuffer<Particle> gParticles : register(t0);

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

VertexShaderOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle p = gParticles[instanceId];

    // パーティクル用バッファは GPU 側で使い回す。未使用スロットは描画対象
    // ではないため、初期化されていない transform を頂点処理へ渡さない。
    if (p.isActive == 0)
    {
        output.position = float4(2.0f, 2.0f, 1.0f, 1.0f);
        output.texcoord = input.uv;
        output.uv = input.uv;
        output.normal = float3(0.0f, 0.0f, 0.0f);
        output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.useNoise = 0;
        output.burnColor = float3(0.0f, 0.0f, 0.0f);
        return output;
    }

    // スケール行列
    matrix scaleMatrix =
    {
        p.scale.x, 0, 0, 0,
        0, p.scale.y, 0, 0,
        0, 0, p.scale.z, 0,
        0, 0, 0, 1
    };


    float c = cos(p.rotate.z);
    float s = sin(p.rotate.z);

    // Z軸回転行列
    matrix particleRotZ =
    {
        c, s, 0, 0,
       -s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    // 回転行列
    matrix rotateMatrix = mul(particleRotZ, billboardMatrix);

    // 平行移動行列
    matrix translateMatrix =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        p.translate.x, p.translate.y, p.translate.z, 1
    };

    // ワールド行列の合成 (S * R * T)
    matrix world = mul(scaleMatrix, mul(rotateMatrix, translateMatrix));
    
    // WVP変換
    output.position = mul(input.position, mul(world, viewProj));

    // =========================================================
    // ★修正箇所：構造体のすべての要素に値を入れる！
    // =========================================================
    
    // 1. texcoord (TEXCOORD0) : ベースのUV座標をそのまま渡す
    output.texcoord = input.uv;

    // 2. uv (TEXCOORD1) : スケール・オフセットを適用したUVを渡す
    output.uv = input.uv * p.uvScale + p.uvOffset;

    // 3. normal (NORMAL0) : パーティクルなので適当な正面方向を入れておく
    output.normal = float3(0.0f, 0.0f, -1.0f);
    
    // ★ 追加：ノイズ設定とふちの色をPSへリレーする
    output.useNoise = p.useNoise;
    output.burnColor = p.burnColor;

    // =========================================================

    // カラー（エミッシブの適用）
    output.color.rgb = p.color.rgb * p.emissive;
    output.color.a = p.color.a;

    return output;
}
