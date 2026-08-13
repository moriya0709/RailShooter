float Random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898f, 78.233f))) * 43758.5453123f);
}

float ValueNoise(float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);

    float a = Random(i);
    float b = Random(i + float2(1.0f, 0.0f));
    float c = Random(i + float2(0.0f, 1.0f));
    float d = Random(i + float2(1.0f, 1.0f));

    float2 u = f * f * (3.0f - 2.0f * f);

    return lerp(a, b, u.x) +
           (c - a) * u.y * (1.0f - u.x) +
           (d - b) * u.x * u.y;
}

float FBM(float2 st)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        value += amplitude * ValueNoise(st);
        st *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

float4 Dissolve(
float4 color, 
float threshold,
float edgeWidth, 
float3 noiseColor,
float3 edgeColor,
float2 uv, 
float noiseScale)
{
    float noise = FBM(uv * noiseScale);

    if (noise < threshold)
    {
        return float4(noiseColor, 1.0f);
    }

    if (noise < threshold + edgeWidth)
    {
        return float4(edgeColor, color.a);
    }

    return color;
}