float4 RadialBlur(
float4 color,
float2 blurCenter,
float blurWidth,
int blurSamples,
float2 uv,
Texture2D<float4> gCurrentTexture,
SamplerState gSampler)
{
    float2 direction = uv - blurCenter;
    float4 blurColor = color;
    for (int i = 1; i < blurSamples; i++)
    {
        float2 offset = direction * blurWidth * float(i);
        blurColor += gCurrentTexture.Sample(gSampler, uv - offset);
    }
    return color = blurColor / float(blurSamples);
}


