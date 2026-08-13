float4 RandomNoise(float2 uv, float time)
{
    float noiseValue = frac(sin(dot(uv + time, float2(12.9898f, 78.233f))) * 43758.5453123f);
    return float4(noiseValue, noiseValue, noiseValue, 1.0f);
}