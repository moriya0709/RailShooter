float4 BoxFilter5x5(float2 uv, Texture2D<float4> gCurrentTexture, SamplerState gSampler)
{
    uint width, height;
    gCurrentTexture.GetDimensions(width, height);
    
    float2 texelSize = float2(1.0f / (float) width, 1.0f / (float) height);

    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 5x5 = 25
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            result += gCurrentTexture.Sample(gSampler, uv + offset);
        }
    }

    return result / 25.0f;
}