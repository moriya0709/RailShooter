// CSで書き込まれた結果のテクスチャ (SRVとしてバインド)
Texture2D<float4> CloudColorTex : register(t0);
Texture2D<float2> CloudVelocityTex : register(t1);

SamplerState PointSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
    float2 Velocity : SV_TARGET1;
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // CSで書き込まれたピクセル結果をサンプリングしてそのまま出力
    output.Color = CloudColorTex.SampleLevel(PointSampler, input.uv, 0);
    output.Velocity = CloudVelocityTex.SampleLevel(PointSampler, input.uv, 0);

    return output;
}