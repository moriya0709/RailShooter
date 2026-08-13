float DetectEdge(Texture2D<float> depthTex, SamplerState samp, float2 uv, float2 texelSize, float threshold)
{
    float2 offsets[9] =
    {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1, 0), float2(0, 0), float2(1, 0),
        float2(-1, 1), float2(0, 1), float2(1, 1)
    };

    float depths[9];
    for (int i = 0; i < 9; ++i)
    {
        depths[i] = depthTex.SampleLevel(samp, saturate(uv + offsets[i] * texelSize), 0).r;
    }

    float Gx = depths[0] - depths[2] + 2.0f * depths[3] - 2.0f * depths[5] + depths[6] - depths[8];
    float Gy = depths[0] + 2.0f * depths[1] + depths[2] - depths[6] - 2.0f * depths[7] - depths[8];

    float edge = sqrt(Gx * Gx + Gy * Gy);

    return step(threshold, edge);
}

float3 Outline(float4 color, float outlineThreshold, float4 outlineColor,float2 uv, Texture2D<float> gDepthTexture, SamplerState gSampler)
{
    uint width, height;
    gDepthTexture.GetDimensions(width, height);
    float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);

    float edge = DetectEdge(
                gDepthTexture,
                gSampler,
                uv,
                texelSize,
                outlineThreshold
            );

    return color.rgb = lerp(color.rgb, outlineColor.rgb, edge * outlineColor.a);
}