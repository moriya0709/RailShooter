float3 Inversion(float4 color)
{
    color.rgb = 1.0f - color.rgb;
    return color.rgb;
}
