float3 DistanceFog(
float4 color,
float3 distanceFogColor,
float distanceFogStart,
float distanceFogEnd,
float zNear,
float zFar,
float2 uv,
Texture2D<float> gDepthTexture,
SamplerState gSampler)
{
    float depth = gDepthTexture.Sample(gSampler, uv);
    float linearDepth = (zNear * zFar) / (zFar - depth * (zFar - zNear));
    float fogFactor = saturate((linearDepth - distanceFogStart) / (distanceFogEnd - distanceFogStart));
    return color.rgb = lerp(color.rgb, distanceFogColor, fogFactor);
}



