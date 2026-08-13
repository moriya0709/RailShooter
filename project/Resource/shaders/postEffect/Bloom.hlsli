// Pass:1
float4 BloomHighBrightness(float4 color, float bloomThreshold)
{
    float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        
    if (brightness > bloomThreshold)
    {
        return color;
    }
    else
    {
        // below the threshold
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

// Pass:2&3
float4 BloomGaussianBlur(
float bloomBlurRadius, 
int gPassId,
float2 uv,
Texture2D<float4> gCurrentTexture,
Texture2D<float4> gBloom3Texture,
SamplerState gSampler)
{
    uint width, height;
    gBloom3Texture.GetDimensions(width, height);
    
    // Texel Size
    float2 texelSize;
    if (width > 0 && height > 0)
        texelSize = 1.0f / float2(width, height);
    else
        texelSize = float2(0.001f, 0.001f);

    // Direction(gPassId = 2:x, gPassId = 3:y)
    float2 direction;
    if (gPassId == 2)
        direction = float2(1.0f, 0.0f);
    else
        direction = float2(0.0f, 1.0f);

    // The center line
    float offset[3] = { 0.0, 1.384615, 3.230769 };
    float weight[3] = { 0.227027, 0.316216, 0.070270 };

    // Sampling
    float3 result = gCurrentTexture.Sample(gSampler, uv).rgb * weight[0];
    for (int i = 1; i < 3; i++)
    {
        float2 uvOffset = direction * texelSize * offset[i] * bloomBlurRadius;
        
        float2 uvSample1 = saturate(uv + uvOffset);
        float2 uvSample2 = saturate(uv - uvOffset);
            
        result += gCurrentTexture.Sample(gSampler, uvSample1).rgb * weight[i];
        result += gCurrentTexture.Sample(gSampler, uvSample2).rgb * weight[i];
    }
    return float4(result, 1.0);
}

float3 BloomComposite(
float4 color,
float bloomIntensity,
float2 uv,
Texture2D<float4> gBloom1Texture,
Texture2D<float4> gBloom2Texture,
Texture2D<float4> gBloom3Texture,
SamplerState gSampler)
{
    float3 b1 = gBloom1Texture.Sample(gSampler, uv).rgb * 1.0f;
    float3 b2 = gBloom2Texture.Sample(gSampler, uv).rgb * 0.4f;
    float3 b3 = gBloom3Texture.Sample(gSampler, uv).rgb * 0.2f;

    float3 totalBloom = b1 + b2 + b3;

    return color.rgb += totalBloom * bloomIntensity;
}