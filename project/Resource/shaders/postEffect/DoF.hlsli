// 深度値から焦点面との距離を求め、円盤状に分布したサンプルで被写界深度のボケを合成する。
float4 DoF(
float4 color,
float zNear,
float zFar,
float focusDistance,
float focusRange,
float bokehRadius,
float2 uv,
Texture2D<float4> gCurrentTexture,
Texture2D<float> gDepthTexture,
SamplerState gSampler)
{
    float depth = gDepthTexture.Sample(gSampler, uv);
    // 深度バッファ値をカメラ空間の距離へ戻し、焦点距離と比較できるようにする。
    float linearDepth = (zNear * zFar) / (zFar - depth * (zFar - zNear));

    float coc = saturate((abs(linearDepth - focusDistance) - focusRange) / bokehRadius);
    float edgeFade = saturate(uv.x * 10.0) * saturate((1.0 - uv.x) * 10.0) *
                             saturate(uv.y * 10.0) * saturate((1.0 - uv.y) * 10.0);
    coc *= edgeFade;
            
    if (coc > 0.0)
    {
        float4 accumColor = 0;
        float totalWeight = 0;
        const int sampleCount = 32;
        // 黄金角を使い、少ないサンプルでも偏りの少ない円盤分布を作る。
        const float GOLDEN_ANGLE = 2.39996323;

        for (int i = 0; i < sampleCount; i++)
        {
            float r = sqrt(float(i) / float(sampleCount));
            float theta = i * GOLDEN_ANGLE;
            float2 offset = float2(cos(theta), sin(theta)) * r * coc * 0.02;
            float2 sampleUV = saturate(uv + offset);
            float4 sampleColor = gCurrentTexture.Sample(gSampler, sampleUV);
                    
            float weight = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
            weight = pow(weight, 2.0) + 0.1;

            accumColor += sampleColor * weight;
            totalWeight += weight;
        }
        return color = accumColor / totalWeight;
    }
    return color;
}
