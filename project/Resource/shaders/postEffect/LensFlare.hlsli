#include "CommonFunctions.hlsli"

float4 LensFlareMain(
int lensFlareGhostCount,
float lensFlareGhostDispersal,
float lensFlareHaloWidth,
float caIntensity,
float2 uv,
Texture2D<float4> gCurrentTexture,
Texture2D<float4> gBloom3Texture,
SamplerState gSampler)
{
	// Variable Declaration
    float2 toCenter = float2(0.5f, 0.5f) - uv;

    uint width, height;
    gCurrentTexture.GetDimensions(width, height);
    float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);

    // aspect(Halo)
    float aspectRatio = (float) width / (float) height;
    float2 uvAspect = float2((uv.x - 0.5f) * aspectRatio, uv.y - 0.5f);

    float3 result = float3(0.0f, 0.0f, 0.0f);
    float dynamicDispersal = lensFlareGhostDispersal;
    float lerpFactor = saturate((dynamicDispersal - 0.2f) / (0.8f - 0.2f));

    // Chromatic Aberration
    float baseCAIntensity = caIntensity;
    caIntensity = baseCAIntensity * lerp(1.0f, 2.0f, lerpFactor);
        
    // Ghost
    int numGhosts = lensFlareGhostCount;
        
    if (numGhosts > 0)
    {
        for (int i = 1; i < numGhosts; ++i)
        {
            float2 offset = uv + toCenter * dynamicDispersal * (float) i;
            float dist = length(0.5f - offset);
            float2 caOffset = toCenter * caIntensity * dist;
                
            float baseSigma = 3.0f + (float) i * 1.5f;
            float blurSigma = baseSigma * lerp(1.0f, 1.2f, lerpFactor);
                
            float r = SampleGaussian(gBloom3Texture, gSampler, saturate(offset + caOffset), texelSize, blurSigma).r;
            float g = SampleGaussian(gBloom3Texture, gSampler, saturate(offset), texelSize, blurSigma).g;
            float b = SampleGaussian(gBloom3Texture, gSampler, saturate(offset - caOffset), texelSize, blurSigma).b;
                
            float weight = pow(1.0f - (float(i) / max(1.0f, float(numGhosts))), 3.0f);
                
            float distFromCenter = length(toCenter);
            float vignette = 1.0f - smoothstep(0.2f, 0.6f, distFromCenter);
                
            result += float3(r, g, b) * weight * vignette * 0.3f;
        }
    }

    //* Halo *//
    float haloRadius = lensFlareHaloWidth;
    float haloThickness = 0.04f;

    float distToCenter = length(uvAspect);

    float innerEdge = haloRadius - haloThickness;
    float outerEdge = haloRadius + haloThickness;
    float weightHalo = smoothstep(innerEdge, haloRadius, distToCenter)
                         - smoothstep(haloRadius, outerEdge, distToCenter);

    float centerFade = smoothstep(0.0f, 0.15f, distToCenter);
    float edgeFade = smoothstep(0.0f, 0.25f, 1.0f - distToCenter * 1.8f);
    weightHalo = saturate(weightHalo) * centerFade * edgeFade;

    if (weightHalo > 0.001f)
    {
        float2 haloOffsetScalar = haloRadius / max(0.001f, length(toCenter));
        float2 haloSampleUV = saturate(uv + toCenter * haloOffsetScalar);
        float3 haloColor = SampleWithCA(gCurrentTexture, gSampler,haloSampleUV, toCenter, caIntensity * 0.5f);
        
        float ringT = saturate((distToCenter - innerEdge) / max(0.0001f, outerEdge - innerEdge));
        float3 haloSpectrum = Spectrum(ringT);

        result += haloColor * haloSpectrum * weightHalo * 2.5f;
    }

    return float4(result, 1.0f);
}

float3 LensFlareComposite(
	float2 uv,
	Texture2D<float4> gLensFlareTexture,
	SamplerState gSampler)
{
    float3 lensFlare = gLensFlareTexture.Sample(gSampler, uv).rgb;
    return lensFlare;
}