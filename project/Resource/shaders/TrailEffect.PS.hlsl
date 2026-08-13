struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float Emissive : TEXCOORD1;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    // テクスチャから色を取得
    float4 texColor = tex.Sample(smp, input.UV);
    
    // テクスチャの色 × 頂点カラー × エミッシブ強度
    float3 finalColor = texColor.rgb * input.Color.rgb * input.Emissive;
    float finalAlpha = texColor.a * input.Color.a;
    
    return float4(finalColor, finalAlpha);
}