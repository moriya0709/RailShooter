struct DS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float shadow : TEXCOORD1;
};

// ルートパラメータ[2]に対応するテクスチャとサンプラを定義
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(DS_OUTPUT input) : SV_TARGET
{
    // テクスチャからUV座標をもとに色をサンプリング（取得）する
    float4 texColor = tex.Sample(smp, input.uv);
    
    // RGB（色）に対して、Domain Shaderで計算した影の明るさを掛ける
    texColor.rgb *= pow(input.shadow, 5.0f);
    
    return texColor;
}