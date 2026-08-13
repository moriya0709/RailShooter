float4 Pinch(
float4 color,
float pinchStrength,
float2 pinchCenter, 
float pinchRadius,
float2 uv,
Texture2D<float4> gCurrentTexture,
SamplerState gSampler)
{
    // Calculate Direction and Distance
    float2 toCenter = uv - pinchCenter;
    float dist = length(toCenter);

    // Determining the Scope of Impact
    if (dist < pinchRadius)
    {
        // Normalization 
        float percent = dist / pinchRadius;         
        // Weight
        float weight = pow(percent, pinchStrength);
            
        uv = pinchCenter + normalize(toCenter) * weight * pinchRadius;
    }
            
    return color = gCurrentTexture.Sample(gSampler, uv);
}