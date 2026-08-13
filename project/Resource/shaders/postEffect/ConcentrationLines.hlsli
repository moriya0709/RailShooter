float3 ConcentrationLines(
    float4 color,
    float concentrationLineIntensity,
    float2 concentrationLineCenter,
    float concentrationLineDensity,
    float concentrationLineLength,
    float concentrationLineSpeed,
    float time,
    float2 uv)
{
    // Polar Coordinates
    float2 toCenter = uv - concentrationLineCenter;
    float dist = length(toCenter);
    float angle = (atan2(toCenter.y, toCenter.x) + 3.14159f) / (2.0f * 3.14159f);

    // Divide Angle
    float lineId = floor(angle * concentrationLineDensity);
        
    // Change the seed
    float timeSeed = floor(time * concentrationLineSpeed);
        
    // Noise
    float n_exists = frac(sin(lineId * 12.9898f + timeSeed) * 43758.5453f);
    float n_length = frac(sin(lineId * 39.1231f + timeSeed) * 753.5453f);
    float n_weight = frac(sin(lineId * 71.3521f + timeSeed) * 921.5453f);

    float lineMask = step(0.5f, n_exists);

    // Variation in Length
    float randomStart = concentrationLineLength + (n_length * 0.2f);
    float distMask = smoothstep(randomStart, randomStart + 0.1f, dist);

    float finalAlpha = lineMask * distMask * concentrationLineIntensity * n_weight;
    float3 lineColor = float3(1, 1, 1);
    
    return color.rgb = lerp(color.rgb, lineColor, finalAlpha);
}