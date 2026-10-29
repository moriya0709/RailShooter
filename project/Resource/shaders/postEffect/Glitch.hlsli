// 簡易的な疑似乱数生成関数
float GlitchRand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// 引数に waveSpeed と waveThreshold を追加
float3 ApplyGlitch(float2 uv, Texture2D tex, SamplerState samp, float time, float intensity, float waveSpeed, float waveThreshold)
{
    // --- アニメーション要素 1: 帯のスクロール ---
    float scrollSpeed = 0.3f;
    float scrolledY = uv.y - (time * scrollSpeed);
    float blockY = floor(scrolledY * 50.0f);

    // --- アニメーション要素 2: 周期的バグ ---
    // 波の速さを引数 waveSpeed で制御
    float glitchWave = sin(time * waveSpeed) * 0.5f + 0.5f;
   
    // エフェクトが止まるタイミングを引数 waveThreshold で制御
    float currentIntensity = intensity * step(waveThreshold, glitchWave);

    float randomNoise = GlitchRand(float2(blockY, time * 15.0f));
    float isShift = step(0.8f, randomNoise);
    float shiftX = (randomNoise - 0.5f) * 0.2f * currentIntensity * isShift;

    float2 shiftedUV = uv + float2(shiftX, 0.0f);

    float shiftR = (GlitchRand(float2(blockY, time * 15.0f + 1.0f)) - 0.5f) * 0.05f * currentIntensity;
    float shiftB = (GlitchRand(float2(blockY, time * 15.0f + 2.0f)) - 0.5f) * 0.05f * currentIntensity;

    float r = tex.Sample(samp, shiftedUV + float2(shiftR, 0.0f)).r;
    float g = tex.Sample(samp, shiftedUV).g;
    float b = tex.Sample(samp, shiftedUV + float2(shiftB, 0.0f)).b;

    return float3(r, g, b);
}