float3 Grayscale(float4 color, int isTwoColor, float threshold, float contrast)
{
    float gray = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    if (isTwoColor)
    {
        // contrast
        gray = saturate((gray - 0.5f) * contrast + 0.5f);
        // Binarization
        gray = smoothstep(threshold - 0.01f, threshold + 0.01f, gray);
    }
    return color.rgb = float3(gray, gray, gray);
}


