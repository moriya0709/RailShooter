#include "rayMarching.hlsli"

// テクスチャおよびサンプラー定義
Texture3D<float4> CloudNoiseTex : register(t0); // 3Dノイズテクスチャ
Texture2D<float> DepthTex : register(t1); // 深度バッファ
SamplerState LinearRepeatSampler : register(s0); // リピート（繰り返し）設定のサンプラー

// CS出力用 UAV (Unordered Access View)
RWTexture2D<float4> OutColor : register(u0); // カラー出力先
RWTexture2D<float2> OutVelocity : register(u1); // 速度ベクトル出力先

cbuffer CloudParam : register(b0)
{
    float4x4 invViewProj;
    float4x4 prevViewProj;

    float3 cameraPos;
    float time; // 時間

    float3 sunDir; // 太陽の位置
    float cloudCoverage; // 雲の密度

    float cloudBottom; // 雲の最低座標
    float cloudTop; // 雲の最高座標
    int isRialLight; // リアル調ライティング
    int isAnimeLight; // アニメ調ライティング
    
    float3 cloudOffset; // uvアニメーション
    int isMotionBlur; // モーションブラー
    
    float cloudOpacity; // 雲の不透明度
    int isStorm; // 雷雨
    float thunderFrequency; // 雷の頻度
    float thunderBrightness; // 雷の明るさ

    float horizonHeight; // 水平線の高さ

    float fogDensity;
    float fogHeight;
    float fogScattering;
    float pad0;
    float3 fogColor;
    float pad1;
}

// ハッシュ
float hash(float3 p)
{
    p = frac(p * 0.3183099 + .1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// ノイズ
float noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    
    f = f * f * (3.0 - 2.0 * f);

    float n = lerp(
        lerp(
            lerp(hash(i + float3(0, 0, 0)), hash(i + float3(1, 0, 0)), f.x),
            lerp(hash(i + float3(0, 1, 0)), hash(i + float3(1, 1, 0)), f.x),
            f.y),
        lerp(
            lerp(hash(i + float3(0, 0, 1)), hash(i + float3(1, 0, 1)), f.x),
            lerp(hash(i + float3(0, 1, 1)), hash(i + float3(1, 1, 1)), f.x),
            f.y),
        f.z);

    return n;
}

// 非整数ブラウン運動
float fbm(float3 p)
{
    float f = 0; // ノイズの合計値
    float amp = 0.5; // 最初のノイズ
    int octaves = 4; // 輪郭のディティール
    float gain = 0.5f; // ノイズの振れ幅

    for (int i = 0; i < octaves; i++)
    {
        f += amp * noise(p);
        p *= 2.0;
        amp *= gain;
    }

    return f;
}

float Remap(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    return newMin + saturate((value - oldMin) / (oldMax - oldMin)) * (newMax - newMin);
}

// 雲の密度を計算する関数
float CloudDensity(float3 p)
{
    float height = (p.y - cloudBottom) / (cloudTop - cloudBottom);

    if (height < 0.0 || height > 1.0)
        return 0.0;

    float coverage = smoothstep(0.0, 0.5, cloudCoverage);
    if (coverage <= 0.0)
        return 0.0;

    float3 pos = p * 0.0005;
    pos.xz += cloudOffset.xz;

    float3 warpOffset = float3(
        fbm(pos + float3(0.0, 0.0, 0.0) + time * 0.01),
        fbm(pos + float3(5.2, 1.3, 2.8) + time * 0.015),
        fbm(pos + float3(1.7, 9.2, 0.4) + time * 0.008)
    );

    float3 warpedPos = pos + warpOffset * 0.8;
    float base = fbm(warpedPos);

    float baseDensity = base - (height * 0.7) - 0.1 + (cloudCoverage * 0.5);
    if (baseDensity + 0.15 <= 0.0)
    {
        return 0.0;
    }

    float detail = noise(warpedPos * 4.0 + time * 0.02) * 0.5
                 + noise(warpedPos * 8.0) * 0.25;

    float localDensity = baseDensity + (detail * 0.25);

    localDensity *= coverage;
    localDensity *= smoothstep(0.0, 0.2, height);
    localDensity *= smoothstep(1.0, 0.6, height);

    return saturate((localDensity - 0.1) * 2.5);
}

// 雲のライティングを計算する関数（リアル調）
float3 RialLightCloud(float3 p)
{
    float3 lightDir = normalize(sunDir);
    float shadow = 0.0;
    float3 pos = p;

    float lightStepLen = 100.0;
    for (int i = 0; i < 4; i++)
    {
        pos += lightDir * lightStepLen;
        shadow += CloudDensity(pos) * lightStepLen * 0.04;
    }

    float3 transmission = 0.0;
    transmission += exp(-shadow);
    transmission += exp(-shadow * 0.25) * 0.7;
    transmission += exp(-shadow * 0.05) * 0.15;

    float powder = 1.0 - exp(-shadow * 2.0);
    transmission *= powder;

    float3 viewDir = normalize(p - cameraPos);
    float cosTheta = dot(-viewDir, lightDir);
    
    float g1 = 0.8;
    float hg1 = (1.0 - g1 * g1) / pow(abs(1.0 + g1 * g1 - 2.0 * g1 * cosTheta), 1.5);
    float g2 = -0.2;
    float hg2 = (1.0 - g2 * g2) / pow(abs(1.0 + g2 * g2 - 2.0 * g2 * cosTheta), 1.5);
    float scatter = lerp(hg1, hg2, 0.5) * 0.25 * 3.14;

    float3 sunColor = float3(1.0, 0.95, 0.85);
    float3 ambientColor = float3(0.25, 0.35, 0.5);

    return sunColor * transmission * scatter + ambientColor * (1.0 - transmission * 0.5);
}

// 雲のライティングを計算する関数（アニメ調）
float3 AnimeLightCloud(float3 p)
{
    float3 lightDir = normalize(sunDir);
    float shadowDensity = 0;
    float3 pos = p;

    float stepLen = 40.0;
    for (int i = 0; i < 5; i++)
    {
        pos += lightDir * stepLen;
        shadowDensity += CloudDensity(pos);
    }

    float transmittance = exp(-shadowDensity * 2.0);
    float toonShadow = smoothstep(0.25, 0.35, transmittance);
    
    float3 litColor = float3(1.0, 0.98, 0.95);
    float3 shadowColor = float3(0.35, 0.45, 0.75);

    return lerp(shadowColor, litColor, toonShadow);
}

// ヘルパー関数: レイと地球（球体）の交差判定
float2 IntersectSphere(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float len = length(ro);
    float c = (len - radius) * (len + radius);
    float h = b * b - c;
    
    if (h < 0.0)
        return float2(-1.0, -1.0);
    
    h = sqrt(h);
    return float2(-b - h, -b + h);
}

// 大気散乱関数 (Nishita Single Scattering 近似)
float3 CalculateAtmosphere(float3 cameraPos, float3 rayDir, float3 sunDir)
{
    float planetRadius = 6371000.0;
    float atmosphereRadius = 6471000.0;

    float safeCameraY = max(cameraPos.y, 0.0);
    float3 ro = float3(0.0, planetRadius + safeCameraY, 0.0);

    float2 atmosphereHit = IntersectSphere(ro, rayDir, atmosphereRadius);
    if (atmosphereHit.y < 0.0)
        return float3(0, 0, 0);

    float tMin = max(0.0, atmosphereHit.x);
    float tMax = atmosphereHit.y;
    
    float2 planetHit = IntersectSphere(ro, rayDir, planetRadius);
    if (planetHit.y > 0.0)
    {
        tMax = min(tMax, max(0.0, planetHit.x));
    }

    float3 rayleighScatteringBase = float3(5.8, 13.5, 33.1) * 1e-6;
    float mieScatteringBase = 21.0 * 1e-6;

    float rayleighScaleHeight = 8000.0;
    float mieScaleHeight = 1200.0;

    int numSteps = 8;
    float stepSize = (tMax - tMin) / float(numSteps);
    float currentPos = tMin + stepSize * 0.5;
    
    float2 opticalDepth = float2(0.0, 0.0);
    float3 totalRayleigh = float3(0, 0, 0);
    float3 totalMie = float3(0, 0, 0);

    static const float PI = 3.14159265;
    static const float INV_4PI = 1.0 / (4.0 * PI);
    
    float cosTheta = dot(rayDir, sunDir);
    float phaseRayleigh = 3.0 / (16.0 * 3.14159) * (1.0 + cosTheta * cosTheta);
    float g = 0.76;
    float g2 = g * g;
    float denom = pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5);
    float phaseMie = (1.0 - g2) * INV_4PI / denom;

    for (int i = 0; i < numSteps; i++)
    {
        float3 samplePos = ro + rayDir * currentPos;
        float height = length(samplePos) - planetRadius;

        float densityRayleigh = exp(-height / rayleighScaleHeight) * stepSize;
        float densityMie = exp(-height / mieScaleHeight) * stepSize;
        opticalDepth += float2(densityRayleigh, densityMie);

        float sunZenithCosAngle = dot(normalize(samplePos), sunDir);
        float sunOpticalDepthR = exp(-height / rayleighScaleHeight) * (1.0 / (max(sunZenithCosAngle, 0.0) + 0.1));
        float sunOpticalDepthM = exp(-height / mieScaleHeight) * (1.0 / (max(sunZenithCosAngle, 0.0) + 0.1));
        float2 opticalDepthLight = float2(sunOpticalDepthR, sunOpticalDepthM) * 8000.0;

        float3 transmittance = exp(-(rayleighScatteringBase * (opticalDepth.x + opticalDepthLight.x) +
                                     mieScatteringBase * (opticalDepth.y + opticalDepthLight.y)));

        totalRayleigh += densityRayleigh * transmittance;
        totalMie += densityMie * transmittance;

        currentPos += stepSize;
    }

    float3 sunColorIntensity = float3(20.0, 20.0, 20.0);

    float3 skyColor = (phaseRayleigh * rayleighScatteringBase * totalRayleigh +
                       phaseMie * mieScatteringBase * totalMie) * sunColorIntensity;

    return skyColor;
}

// 雷の発光を計算する関数
float3 CalculateLightning(float3 pos, float time, float3 cameraPos, float cloudBottom)
{
    float3 totalLightning = float3(0, 0, 0);
    
    for (int i = 0; i < 2; i++)
    {
        float t = time * thunderFrequency + float(i) * 5.31;
        float seed = floor(t);
        
        float localTime = frac(t);
        float flash = smoothstep(0.0, 0.02, localTime) * smoothstep(0.15, 0.02, localTime);
        flash *= saturate(sin(time * 80.0 + float(i) * 12.0));
        
        if (flash <= 0.0)
            continue;
        
        float2 offset = float2(
            (hash(float3(seed, i, 0)) - 0.5) * 30000.0,
            (hash(float3(seed, i, 1)) - 0.5) * 30000.0
        );
        float3 lightningPos = cameraPos + float3(offset.x, cloudBottom + 800.0, offset.y);
        
        float dist = length(pos - lightningPos);
        float atten = exp(-dist * 0.001);
        
        totalLightning += float3(0.6, 0.8, 1.0) * flash * atten * thunderBrightness;
    }
    
    return totalLightning;
}

// フォグの密度計算
float GetFogDensity(float3 p)
{
    float heightFactor = saturate((fogHeight - p.y) / max(fogHeight, 0.001));
    
    float3 uv = p * 0.02;
    uv.xz += cloudOffset.xz * 2.0;
    
    float noiseVal = fbm(uv);
    
    return heightFactor * heightFactor * (noiseVal * 0.5 + 0.5) * fogDensity;
}

// 太陽光の遮蔽度計算
float GetSunVisibility(float3 p, float3 sunDir)
{
    float shadowStepLen = 40.0;
    float totalCloudDensity = 0.0;
    
    for (int i = 1; i <= 4; i++)
    {
        float3 shadowP = p + sunDir * (float(i) * shadowStepLen);
        totalCloudDensity += CloudDensity(shadowP);
    }
    
    return exp(-totalCloudDensity * 0.8);
}

// コンピュートシェーダー エントリーポイント (8x8のスレッドグループ)
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelPos = DTid.xy;

    // 出力テクスチャの解像度を取得して画面外判定
    float2 texSize;
    OutColor.GetDimensions(texSize.x, texSize.y);

    if (pixelPos.x >= (uint) texSize.x || pixelPos.y >= (uint) texSize.y)
        return;

    // ピクセル中央のUV座標(0.0 ~ 1.0)を算出
    float2 uv = (float2(pixelPos) + 0.5) / texSize;

    float2 ndcXY = uv * 2.0f - 1.0f;
    ndcXY.y *= -1.0f;
    float4 clip = float4(ndcXY, 1.0, 1.0);
    float4 world = mul(invViewProj, clip);
    world.xyz /= world.w;
    float3 rayDir = normalize(world.xyz - cameraPos);

    // 深度バッファを読み込み、オブジェクトのワールド距離を計算
    float depth = DepthTex.SampleLevel(LinearRepeatSampler, uv, 0).r;
    float4 clipObj = float4(ndcXY, depth, 1.0);
    float4 worldObj = mul(invViewProj, clipObj);
    worldObj.xyz /= worldObj.w;

    float objDist = length(worldObj.xyz - cameraPos);
    if (depth >= 1.0)
    {
        objDist = 50000.0;
    }
    
    // 大気散乱
    float3 normalizedSunDir = normalize(-sunDir);

    float3 skyRayDir = normalize(rayDir + float3(0, horizonHeight, 0));
    float3 skyColor = CalculateAtmosphere(cameraPos, skyRayDir, normalizedSunDir);

    float3 ambientRayDir = normalize(float3(rayDir.x, max(rayDir.y, 0.05), rayDir.z));
    float3 cloudAmbientSkyColor = CalculateAtmosphere(cameraPos, ambientRayDir, normalizedSunDir);

    float sunHeight = normalizedSunDir.y;
    float dayFactor = saturate(sunHeight * 4.0);
    float sunsetTime = smoothstep(0.3, 0.0, sunHeight)
                     * smoothstep(-0.2, 0.0, sunHeight);

    if (isStorm)
    {
        skyColor = lerp(skyColor, float3(0.03, 0.04, 0.05), 0.9);
        cloudAmbientSkyColor = lerp(cloudAmbientSkyColor, float3(0.04, 0.05, 0.06), 0.9);
        dayFactor *= 0.13;
    }
    
    // 水平線
    if (skyRayDir.y < 0.0)
    {
        float3 daySea = float3(0.05, 0.3, 0.6);
        float3 sunsetSea = float3(0.6, 0.25, 0.1);
        float3 nightSea = float3(0.01, 0.015, 0.03);
        float3 seaColor;
        if (sunHeight > 0.0)
            seaColor = lerp(sunsetSea, daySea, smoothstep(0.0, 0.2, sunHeight));
        else
            seaColor = lerp(nightSea, sunsetSea, smoothstep(-0.2, 0.0, sunHeight));
        seaColor += cloudAmbientSkyColor * 0.1;
        skyColor = lerp(seaColor, skyColor, smoothstep(-0.05, 0.0, skyRayDir.y));
    }

    float horizonBand = smoothstep(0.15, 0.0, abs(skyRayDir.y));

    float3 horizonDay = float3(0.4, 0.65, 1.0);
    float3 horizonNight = float3(0.02, 0.04, 0.12);
    float3 baseHorizonColor = lerp(horizonNight, horizonDay, saturate(sunHeight * 4.0 + 0.5));

    float3 horizonSunset = float3(1.0, 0.45, 0.1);

    float sunAlignH = saturate(dot(normalize(float2(skyRayDir.x, skyRayDir.z)),
                                   normalize(float2(normalizedSunDir.x, normalizedSunDir.z))));

    float sunsetIntensity = smoothstep(0.3, 0.0, abs(sunHeight)) * pow(sunAlignH, 4.0);

    float3 horizonColor = lerp(baseHorizonColor, horizonSunset, sunsetIntensity);

    float horizonGlow = saturate(horizonBand * 1.5);
    skyColor = lerp(skyColor, horizonColor, horizonGlow);

    // 雲の交差判定
    float3 color = float3(0, 0, 0);
    float transmittance = 1.0;
    bool hitClouds = true;

    if (abs(rayDir.y) < 0.0001)
        hitClouds = false;

    float tBottom = (cloudBottom - cameraPos.y) / rayDir.y;
    float tTop = (cloudTop - cameraPos.y) / rayDir.y;
    float tStart = min(tBottom, tTop);
    float tEnd = min(max(tBottom, tTop), objDist);
    if (tEnd < 0 || tStart >= tEnd)
        hitClouds = false;

    // レイマーチング
    if (hitClouds)
    {
        tStart = max(tStart, 0);
        float effectiveEnd = min(tStart + 20000.0, tEnd);

        float minStep = 100.0;
        float maxStep = 600.0;

        float3 pos = cameraPos + rayDir * tStart;
        float randomJitter = frac(sin(dot(uv, float2(127.1, 311.7)))
                           * 43758.5 + time * 0.1);
        pos += rayDir * (minStep * randomJitter * 0.5);

        float t = tStart;

        for (int i = 0; i < 48; i++)
        {
            if (t >= effectiveEnd)
                break;

            float d = CloudDensity(pos);

            float stepLen = (d < 0.01) ? maxStep : minStep;
            float opticalDepth = d * stepLen * cloudOpacity;

            if (opticalDepth > 0.001)
            {
                float cosTheta = dot(rayDir, normalizedSunDir);
                float g = 0.7;
                float phase = (1.0 - g * g)
                                 / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5)
                                 / (4.0 * 3.14159);
                float phaseBlend = lerp(phase, 1.0, 0.3);

                float powderEffect = 1.0 - exp(-opticalDepth * 2.0);
                float lightScattering = exp(-d * 50.0) * powderEffect * phaseBlend;

                float sunsetSpread = smoothstep(-1.0, 1.0, cosTheta);
                float3 wideSunsetColor = float3(1.0, 0.35, 0.1) * sunsetSpread * sunsetTime;

                float3 daySunColor = float3(1.0, 0.92, 0.85);
                float3 sunsetSunColor = float3(1.0, 0.45, 0.05);
                float3 nightSunColor = float3(0.08, 0.10, 0.18);
                float3 currentSunColor = lerp(daySunColor, sunsetSunColor, sunsetTime);
                currentSunColor = lerp(nightSunColor, currentSunColor, dayFactor);

                float sunIntensity = lerp(0.2, 7.0, dayFactor);
                float3 sunIllumination = currentSunColor * sunIntensity * lightScattering;

                float3 nightAmbient = float3(0.015, 0.02, 0.05);
                float skyLuma = dot(cloudAmbientSkyColor, float3(0.299, 0.587, 0.114));
                float3 desaturatedSky = lerp(float3(skyLuma, skyLuma, skyLuma),
                                             cloudAmbientSkyColor, 0.2);
                float3 dayAmbient = (desaturatedSky * 0.1) + (wideSunsetColor * 0.4);
                float3 cloudAmbient = lerp(nightAmbient, dayAmbient, dayFactor);

                float3 light = float3(0, 0, 0);
                if (isRialLight)
                    light = RialLightCloud(pos);
                if (isAnimeLight)
                    light = AnimeLightCloud(pos);

                light = light * lerp(0.05, 1.0, dayFactor);
                light += sunIllumination + cloudAmbient;

                if (isStorm)
                {
                    float3 lightning = CalculateLightning(pos, time, cameraPos, cloudBottom);
                    light += lightning * exp(-d * 1.5);
                }
                
                color += opticalDepth * light * transmittance;
                transmittance *= exp(-opticalDepth);
                if (transmittance < 0.005)
                    break;
            }

            pos += rayDir * stepLen;
            t += stepLen;
        }
    }
    
    // ボリューメトリックフォグのレイマーチング
    float3 fogAccumColor = float3(0, 0, 0);
    float fogTransmittance = 1.0;
    
    float tMin = 0.0;
    float tMax = min(objDist, 2000.0);
    
    if (abs(rayDir.y) > 0.001)
    {
        float tFog = (fogHeight - cameraPos.y) / rayDir.y;
        if (rayDir.y > 0.0)
        {
            tMax = min(tMax, max(tFog, 0.0));
        }
        else
        {
            tMin = max(tMin, tFog);
        }
    }
    
    bool hitFog = (tMin < tMax);
    
    if (hitFog && fogDensity > 0.0)
    {
        int fogSteps = 24;
        
        float fogDistance = tMax - tMin;
        float fogStepLen = fogDistance / float(fogSteps);
        
        float randomJitterFog = frac(sin(dot(uv, float2(127.1, 311.7))) * 43758.5 + time * 0.2);
        
        float fogT = tMin + fogStepLen * randomJitterFog;
        float fogMaxDist = tMax;
        
        for (int j = 0; j < fogSteps; j++)
        {
            if (fogT >= fogMaxDist || fogTransmittance < 0.01)
                break;
        
            float3 p = cameraPos + rayDir * fogT;
    
            if (p.y > fogHeight)
            {
                fogT += fogStepLen;
                continue;
            }
    
            float d = GetFogDensity(p);
            if (d > 0.001)
            {
                float opticalDepth = d * fogStepLen;
                float sunVis = GetSunVisibility(p, normalizedSunDir);
        
                float cosTheta = dot(rayDir, normalizedSunDir);
                float g = 0.6;
                float phase = (1.0 - g * g) / pow(abs(1.0 + g * g - 2.0 * g * cosTheta), 1.5) / (4.0 * 3.14159);
        
                float3 sunIllum = lerp(float3(1.0, 0.9, 0.8), float3(1.0, 0.4, 0.05), sunsetTime) * dayFactor * 3.0;
                float3 ambientIllum = fogColor * cloudAmbientSkyColor;
        
                float3 directLight = sunIllum * phase * sunVis;
                float3 stepLight = (directLight + ambientIllum) * d;
        
                fogAccumColor += stepLight * fogTransmittance * fogStepLen;
                fogTransmittance *= exp(-opticalDepth);
            }
            fogT += fogStepLen;
        }
    }

    // 最終合成
    float3 backgroundAndClouds = color;
    if (depth >= 1.0)
    {
        backgroundAndClouds += skyColor * transmittance;
    }
    
    float3 finalColor = backgroundAndClouds * fogTransmittance + fogAccumColor;
    
    float totalTransmittance = transmittance * fogTransmittance;
    float outAlpha = 1.0 - totalTransmittance;
    
    if (depth >= 1.0)
    {
        outAlpha = 1.0;
    }

    float exposure = lerp(0.6, 1.5, dayFactor);
    finalColor = 1.0 - exp(-finalColor * exposure);
    finalColor = max(finalColor, 0.0);
    
    float contrast = 1.5;
    finalColor = pow(finalColor, float3(contrast, contrast, contrast));
    finalColor = finalColor * finalColor * (3.0 - 2.0 * finalColor);

    float saturation = lerp(0.7, 1.2, dayFactor);
    float luminance = dot(finalColor, float3(0.299, 0.587, 0.114));
    finalColor = lerp(float3(luminance, luminance, luminance), finalColor, saturation);

    // 太陽ディスク（HDR）の描画
    if (depth >= 1.0)
    {
        float3 colDay = float3(60.0, 55.0, 45.0);
        float3 colGolden = float3(80.0, 45.0, 5.0);
        float3 colSunset = float3(100.0, 15.0, 2.0);
        float3 colMoon = float3(0.5, 0.8, 2.0);

        float3 dynamicSunColor;
        if (sunHeight > 0.2)
        {
            dynamicSunColor = lerp(colGolden, colDay, smoothstep(0.2, 0.6, sunHeight));
        }
        else if (sunHeight > 0.0)
        {
            dynamicSunColor = lerp(colSunset, colGolden, smoothstep(0.0, 0.2, sunHeight));
        }
        else
        {
            dynamicSunColor = lerp(colMoon, colSunset, smoothstep(-0.1, 0.0, sunHeight));
        }

        float sunDot = dot(skyRayDir, normalizedSunDir);
        float sunDisc = smoothstep(0.9998f, 0.99995f, sunDot);
        float sunAlpha_ = (sunHeight > 0.0) ? 1.0 : 0.2;
        if (isStorm)
            sunAlpha_ = 0.0;
        
        finalColor += dynamicSunColor * sunDisc * transmittance * sunAlpha_;
    }

    if (depth < 1.0 && outAlpha > 0.001)
    {
        finalColor /= outAlpha;
    }

    // モーションブラー
    float2 velocity = float2(0, 0);
    if (isMotionBlur)
    {
        float4 prevClip = mul(prevViewProj, float4(world.xyz, 1.0));
        prevClip.xyz /= prevClip.w;

        float2 prevUV = prevClip.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

        velocity = uv - prevUV;
        float cloudBlurStrength = 5.0;
        velocity *= (1.0 - transmittance) * cloudBlurStrength;
    }

    // ★ UAV（テクスチャ）へ結果を出力
    OutColor[pixelPos] = float4(finalColor, outAlpha);
    OutVelocity[pixelPos] = velocity;
}