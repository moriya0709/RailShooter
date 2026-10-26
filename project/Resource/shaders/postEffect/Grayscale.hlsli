float3 Grayscale(float4 color, int isTwoColor, float threshold, float contrast, float progress)
{
    float gray = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    if (isTwoColor)
    {
        // contrast
        gray = saturate((gray - 0.5f) * contrast + 0.5f);
        // Binarization
        gray = smoothstep(threshold - 0.01f, threshold + 0.01f, gray);
    }
    
    // progressが0.0なら元の色、1.0なら白黒になるように補間する
    float3 grayscaleColor = float3(gray, gray, gray);
    return lerp(color.rgb, grayscaleColor, progress);
}