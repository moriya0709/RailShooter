float4 SpeedDistortion(float4 color, float speedDistortionStrength, float2 uv, Texture2D<float4> gCurrentTexture, SamplerState gSampler)
{
    // Vector
    float2 toCenter = uv - float2(0.5f, 0.5f);
            
    // Distortion
    float distSq = dot(toCenter, toCenter);
    float warpFactor = 1.0f + (speedDistortionStrength * distSq);
    uv = float2(0.5f, 0.5f) + toCenter * warpFactor;
            
    // Preventing Overflow
    uv = saturate(uv);
            
    // Resample the base color
    return color = gCurrentTexture.Sample(gSampler, uv);
}