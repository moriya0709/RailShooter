float3 EyeBlink(float3 color, float2 uv, float progress)
{
    float distX = uv.x - 0.5f;
    float distY = abs(uv.y - 0.5f);
    
    float curveStrength = 0.6f;
    
    float curve = (distX * distX) * curveStrength;
    
    float finalDistY = distY + curve;
    
    float threshold = 0.5f - (progress * 0.5f);
    
    float blink = smoothstep(threshold, threshold + 0.1f, finalDistY);
    
    return lerp(color, float3(0.0f, 0.0f, 0.0f), blink);
}