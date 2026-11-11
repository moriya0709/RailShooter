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
    // RG はコンピュートシェーダーが生成した速度。A は既存の速度との合成率。
    float4 Velocity : SV_TARGET1;
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // CSで書き込まれたピクセル結果をサンプリングしてそのまま出力
    output.Color = CloudColorTex.SampleLevel(PointSampler, input.uv, 0);

    // 雲がない前景では既存のオブジェクト速度を残す。BlendState 側で alpha を
    // 掛けるため、コンピュートシェーダーが作った速度の強さを保つよう事前に補正する。
    const float coverage = saturate(output.Color.a);
    const float2 cloudVelocity = CloudVelocityTex.SampleLevel(PointSampler, input.uv, 0);
    output.Velocity = float4(cloudVelocity / max(coverage, 0.001f), 0.0f, coverage);

    return output;
}
