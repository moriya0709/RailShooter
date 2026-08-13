float3 Vignette(float4 color, float vignetteIntensity, float3 vignetteColor,float2 uv)
{
    // Distance from the center
    float dist = distance(uv, float2(0.5f, 0.5f));
            
    // Gradient
    float vignetteWeight = smoothstep(0.3f, 0.8f, dist);
    vignetteWeight *= saturate(vignetteIntensity);

    // Color
    float3 damageColor = vignetteColor;

    return color.rgb = lerp(color.rgb, damageColor, vignetteWeight);
}