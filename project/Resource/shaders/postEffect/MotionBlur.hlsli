float4 MotionBlur(
float4 color,
int motionBlurSamples,
float motionBlurScale,
float2 uv,
Texture2D<float4> gCurrentTexture,
Texture2D<float2> gVelocityTexture,
SamplerState gSampler)
{
	// Speed adjustment
    float2 velocity = gVelocityTexture.Sample(gSampler, uv).rg;
    velocity *= motionBlurScale;

    if (length(velocity) > 0.0001f) // Weight reduction
    {
        // Displacement per sample
        float2 texelStep = velocity / (float) motionBlurSamples;
            
        float4 accumColor = color;
        float2 currentUV = uv;

        // Sampling
        for (int i = 1; i < motionBlurSamples; ++i)
        {
            currentUV -= texelStep;
                
            // Clamp
            currentUV = saturate(currentUV);
            accumColor += gCurrentTexture.Sample(gSampler, currentUV);
        }
            
        // Leveling
        return color = accumColor / (float) motionBlurSamples;
    }
    
    return color;
}