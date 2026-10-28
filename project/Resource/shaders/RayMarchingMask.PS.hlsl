Texture2D<float4> CloudColorTex : register(t0);
SamplerState PointSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    const float alpha = CloudColorTex.SampleLevel(PointSampler, input.uv, 0).a;
    clip(alpha - 0.001f);
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
