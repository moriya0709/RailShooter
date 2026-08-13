float3 HeightFog(
float4 color,
float3 heightFogColor,
float heightFogTop,
float heightFogBottom,
float heightFogDensity,
float4x4 matInverseViewProjection,
float2 uv,
Texture2D<float> gDepthTexture,
SamplerState gSampler)
{
    float depth = gDepthTexture.Sample(gSampler, uv);
    float2 ndcXY = uv * 2.0f - 1.0f;
    ndcXY.y *= -1.0f;
    float4 ndcPos = float4(ndcXY, depth, 1.0f);
    float4 worldPosWithW = mul(matInverseViewProjection, ndcPos);
    float3 worldPos = worldPosWithW.xyz / worldPosWithW.w;

    float heightFactor = saturate((heightFogTop - worldPos.y) / (heightFogTop - heightFogBottom));
    heightFactor = pow(heightFactor, heightFogDensity);
    return color.rgb = lerp(color.rgb, heightFogColor, heightFactor);
}



