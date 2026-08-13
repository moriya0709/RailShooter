// Gaussian blur
float3 SampleGaussian(Texture2D<float4> tex, SamplerState samp, float2 uv, float2 texelSize, float blurSigma)
{
    float3 result = 0;
    float totalWeight = 0;
    const float kernel[3] = { 0.227027, 0.316216, 0.070270 };
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize * blurSigma;
            float weight = kernel[abs(x)] * kernel[abs(y)];
            result += tex.Sample(samp, uv + offset).rgb * weight;
            totalWeight += weight;
        }
    }
    return result / totalWeight;
}

// Chromatic Aberration
float3 SampleWithCA(Texture2D<float4> tex, SamplerState samp,
                    float2 uv, float2 toCenter, float caIntensity)
{
    float dist = length(toCenter);
    float caScale = caIntensity * dist * dist * 8.0f;
    float2 caDir = normalize(toCenter + 0.0001f);

    float r = tex.Sample(samp, saturate(uv + caDir * caScale * 1.0f)).r;
    float g = tex.Sample(samp, saturate(uv + caDir * caScale * 0.5f)).g;
    float b = tex.Sample(samp, saturate(uv - caDir * caScale * 0.5f)).b;
    return float3(r, g, b);
}

// Spectral Gradient
float3 Spectrum(float t)
{
    float3 r = float3(1.0, 0.0, 0.0); // Red
    float3 g = float3(0.0, 1.0, 0.0); // Green
    float3 b = float3(0.0, 0.0, 1.0); // Blue

    float3 color = cos((t - float3(0.0, 0.5, 1.0)) * 3.14159 * 2.0) * 0.5 + 0.5;
    return color;
}

// ACES
float3 ACESFitted(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}