#include "rayMarching.hlsli"

// 新しく追加する定義
Texture3D<float4> CloudNoiseTex : register(t0); // 3Dノイズテクスチャ
Texture2D<float> DepthTex : register(t1); // 深度バッファ
SamplerState LinearRepeatSampler : register(s0); // リピート（繰り返し）設定のサンプラー

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
struct PSOutput
{
    float4 Color : SV_TARGET0;
    float2 Velocity : SV_TARGET1;
};

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

float CloudDensity(float3 p)
{
    float height = (p.y - cloudBottom) / (cloudTop - cloudBottom);

    if (height < 0.0 || height > 1.0)
        return 0.0;

    // カバレッジ計算を先にやり、完全に雲が無い場合は計算ストップ
    float coverage = smoothstep(0.0, 0.5, cloudCoverage);
    if (coverage <= 0.0)
        return 0.0;

    float3 uv = p * 0.001;
    uv += cloudOffset * 3.0;

    // ベース形状だけを取得
    float base = fbm(uv);

    // ディテールを足す前の仮の密度を計算する
    float baseDensity = base - (height * 0.8) - 0.1 + (cloudCoverage * 0.5);
    if (baseDensity + 0.15 <= 0.0)
    {
        return 0.0;
    }

    // ここまで生き残った場所（雲の内部や輪郭）だけ、2回目のノイズを計算
    float detail = noise(uv * 4.0) * 0.5 + noise(uv * 8.0) * 0.25;

    // 仮の密度にディテールを合成
    float localDensity = baseDensity + (detail * 0.3);

    // マスクを掛けて、隙間を確定させる
    localDensity *= coverage;

    // 高度による制限
    localDensity *= smoothstep(0.0, 0.15, height);
    localDensity *= smoothstep(1.0, 0.7, height);

    // 強めにしきい値を設定して、空間をパキッと分ける
    return saturate((localDensity - 0.1) * 2.0);
}

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

    // 多重散乱の近似 (光の減衰を複数のオクターブで計算)
    float3 transmission = 0.0;
    transmission += exp(-shadow);
    transmission += exp(-shadow * 0.25) * 0.7;
    transmission += exp(-shadow * 0.05) * 0.15;

    // パウダーエフェクト（Beer-Powder）
    float powder = 1.0 - exp(-shadow * 2.0);
    transmission *= powder;

    // Henyey-Greenstein 近似
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
    
    // 巨大な数同士の引き算による桁落ち（精度エラー）を防ぐ数学的トリック
    float len = length(ro);
    float c = (len - radius) * (len + radius);
    
    float h = b * b - c;
    
    // 交差しない場合
    if (h < 0.0)
        return float2(-1.0, -1.0);
    
    h = sqrt(h);
    return float2(-b - h, -b + h); // 交差する2点までの距離
}

// 大気散乱関数 (Nishita Single Scattering 近似)
float3 CalculateAtmosphere(float3 cameraPos, float3 rayDir, float3 sunDir)
{
    // 地球と大気のスケール設定（1単位 = 1メートル想定）
    float planetRadius = 6371000.0; // 地球の半径（約6371km）
    float atmosphereRadius = 6471000.0; // 大気圏の果て（上空100km）

    // カメラが地下に潜っても、計算上の最低高度を0（地上）に保つ
    float safeCameraY = max(cameraPos.y, 0.0);
    
    // カメラ位置を「地球の表面付近」として計算空間に合わせる
    float3 ro = float3(0.0, planetRadius + safeCameraY, 0.0);

    // 大気圏との交差判定（どこまで計算するか）
    float2 atmosphereHit = IntersectSphere(ro, rayDir, atmosphereRadius);
    if (atmosphereHit.y < 0.0)
        return float3(0, 0, 0); // 大気圏外を向いている

    float tMin = max(0.0, atmosphereHit.x);
    float tMax = atmosphereHit.y;
    
    // 視線が地面（地球）にぶつかる、または表面スレスレ（内部判定）の場合
    float2 planetHit = IntersectSphere(ro, rayDir, planetRadius);
    // x(入口)ではなく、y(出口)が前方にあるか(> 0.0)で「地球と重なっているか」を判定する
    if (planetHit.y > 0.0)
    {
        // 交差点(tMax)を入口(x)に設定するが、マイナス(誤差)の場合は 0.0 にクランプする
        tMax = min(tMax, max(0.0, planetHit.x));
    }

    // 散乱係数（RGBの波長ごとの散乱の強さ）
    float3 rayleighScatteringBase = float3(5.8, 13.5, 33.1) * 1e-6; // 青い光ほど強く散乱する
    float mieScatteringBase = 21.0 * 1e-6; // 白っぽい霞み

    // スケールハイト（大気の密度が 1/e になる高度）
    float rayleighScaleHeight = 8000.0; // レイリー散乱は高度8km
    float mieScaleHeight = 1200.0; // ミー散乱は高度1.2km

    // 計算の分割数（背景用なので16回で十分綺麗です）
    int numSteps = 8;
    float stepSize = (tMax - tMin) / float(numSteps);
    float currentPos = tMin + stepSize * 0.5;
    
    float2 opticalDepth = float2(0.0, 0.0);
    float3 totalRayleigh = float3(0, 0, 0);
    float3 totalMie = float3(0, 0, 0);

    // 係数を事前計算
    static const float PI = 3.14159265;
    static const float INV_4PI = 1.0 / (4.0 * PI);
    
    // 位相関数（光が視線に向かってどう散乱するか）
    float cosTheta = dot(rayDir, sunDir);
    // レイリー位相関数（空全体に広がる光）
    float phaseRayleigh = 3.0 / (16.0 * 3.14159) * (1.0 + cosTheta * cosTheta);
    // ミー位相関数（太陽の周りに強く出る光）
    float g = 0.76;
    float g2 = g * g;
    float denom = pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5);
    float phaseMie = (1.0 - g2) * INV_4PI / denom;
    // 大気の中を進みながら光を足し合わせるループ
    for (int i = 0; i < numSteps; i++)
    {
        float3 samplePos = ro + rayDir * currentPos;
        float height = length(samplePos) - planetRadius;

        // 現在の高度での空気の密度
        float densityRayleigh = exp(-height / rayleighScaleHeight) * stepSize;
        float densityMie = exp(-height / mieScaleHeight) * stepSize;
        opticalDepth += float2(densityRayleigh, densityMie);

        // 太陽からの光の減衰（簡易近似）
        // ※太陽が低いほど分厚い大気を通るため、青が散乱しきって赤（夕焼け）が残る
        float sunZenithCosAngle = dot(normalize(samplePos), sunDir);
        float sunOpticalDepthR = exp(-height / rayleighScaleHeight) * (1.0 / (max(sunZenithCosAngle, 0.0) + 0.1));
        float sunOpticalDepthM = exp(-height / mieScaleHeight) * (1.0 / (max(sunZenithCosAngle, 0.0) + 0.1));
        float2 opticalDepthLight = float2(sunOpticalDepthR, sunOpticalDepthM) * 8000.0;

        // 光の透過率
        float3 transmittance = exp(-(rayleighScatteringBase * (opticalDepth.x + opticalDepthLight.x) +
                                     mieScatteringBase * (opticalDepth.y + opticalDepthLight.y)));

        // 散乱した光を蓄積
        totalRayleigh += densityRayleigh * transmittance;
        totalMie += densityMie * transmittance;

        currentPos += stepSize;
    }

    // 太陽の光の強さ（数値を上げると全体が眩しくなります）
    float3 sunColorIntensity = float3(20.0, 20.0, 20.0);

    // 最終的な空の色を合成
    float3 skyColor = (phaseRayleigh * rayleighScatteringBase * totalRayleigh +
                       phaseMie * mieScatteringBase * totalMie) * sunColorIntensity;

    return skyColor;
}

// 雷の発光を計算する関数
float3 CalculateLightning(float3 pos, float time, float3 cameraPos, float cloudBottom)
{
    float3 totalLightning = float3(0, 0, 0);
    
    // 2箇所の雷を別々のタイミング・場所で発生させる
    for (int i = 0; i < 2; i++)
    {
        // 時間軸をスケールし、数秒に1回雷が鳴るようにする
        float t = time * thunderFrequency + float(i) * 5.31;
        float seed = floor(t); // シード値（発光ごとに位置が変わる）
        
        // ピカッと光るパルス波形（一瞬だけ1.0に近づく）
        float localTime = frac(t);
        float flash = smoothstep(0.0, 0.02, localTime) * smoothstep(0.15, 0.02, localTime);
        
        // サイン波でチカチカさせる（フリッカー効果）
        flash *= saturate(sin(time * 80.0 + float(i) * 12.0));
        
        // 発光しないタイミングなら計算をスキップして軽量化
        if (flash <= 0.0)
            continue;
        
        // 雷の中心位置をシード値からランダムに決定（カメラ周囲 約15km圏内）
        float2 offset = float2(
            (hash(float3(seed, i, 0)) - 0.5) * 30000.0,
            (hash(float3(seed, i, 1)) - 0.5) * 30000.0
        );
        float3 lightningPos = cameraPos + float3(offset.x, cloudBottom + 800.0, offset.y);
        
        // 現在のレイ位置と雷の中心位置の距離で光を減衰させる
        float dist = length(pos - lightningPos);
        float atten = exp(-dist * 0.001); // 値が小さいほど遠くまで光が届く
        
        // 青白い強烈な光を合成（係数で明るさを調整）
        totalLightning += float3(0.6, 0.8, 1.0) * flash * atten * thunderBrightness;
    }
    
    return totalLightning;
}

PSOutput main(VSOutput input)
{
    float2 ndcXY = input.uv * 2.0f - 1.0f;
    ndcXY.y *= -1.0f;
    float4 clip = float4(ndcXY, 1.0, 1.0);
    float4 world = mul(invViewProj, clip);
    world.xyz /= world.w;
    float3 rayDir = normalize(world.xyz - cameraPos);

    // 深度バッファを読み込み、オブジェクトのワールド距離を計算
    float depth = DepthTex.SampleLevel(LinearRepeatSampler, input.uv, 0).r;
    float4 clipObj = float4(ndcXY, depth, 1.0);
    float4 worldObj = mul(invViewProj, clipObj);
    worldObj.xyz /= worldObj.w;

    float objDist = length(worldObj.xyz - cameraPos);
    // 深度が1.0（無限遠・背景）の場合は、距離を非常に遠く設定
    if (depth >= 1.0)
    {
        objDist = 50000.0;
    }
    
    // *大気散乱* //
    float3 normalizedSunDir = normalize(-sunDir);

    float3 skyRayDir = normalize(rayDir + float3(0, horizonHeight, 0));
    float3 skyColor = CalculateAtmosphere(cameraPos, skyRayDir, normalizedSunDir);

    float3 ambientRayDir = normalize(float3(rayDir.x, max(rayDir.y, 0.05), rayDir.z));
    float3 cloudAmbientSkyColor = CalculateAtmosphere(cameraPos, ambientRayDir, normalizedSunDir);

    // ★ 昼夜判定（normalizedSunDir.yが正=昼、負=夜）
    float sunHeight = normalizedSunDir.y;
    float dayFactor = saturate(sunHeight * 4.0); // 0=夜、1=昼
    float sunsetTime = smoothstep(0.3, 0.0, sunHeight)
                     * smoothstep(-0.2, 0.0, sunHeight); // 夕焼けピーク

    if (isStorm)
    {
        // 昼間でも空をどんよりとした暗いグレーにする
        skyColor = lerp(skyColor, float3(0.03, 0.04, 0.05), 0.9);
        cloudAmbientSkyColor = lerp(cloudAmbientSkyColor, float3(0.04, 0.05, 0.06), 0.9);
        dayFactor *= 0.13; // 環境光と太陽の光を大幅に抑えて雲を黒くする
    }
    
    // *水平線* //
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

    // 地平線グロー（昼夜で変化）
    float horizonBand = smoothstep(0.15, 0.0, abs(skyRayDir.y));

    // 基本の水平線カラー（常に青～夜の色）
    float3 horizonDay = float3(0.4, 0.65, 1.0); // 綺麗な青色に調整
    float3 horizonNight = float3(0.02, 0.04, 0.12);
    // sunHeightを元に昼と夜の基本色をブレンド（夕焼け色は入れない）
    float3 baseHorizonColor = lerp(horizonNight, horizonDay, saturate(sunHeight * 4.0 + 0.5));

    // 夕焼け色（太陽の周りだけに使用）
    float3 horizonSunset = float3(1.0, 0.45, 0.1);

    // 視線と太陽方向の水平方向の一致度
    // （normalizeを追加して精度を安定させています）
    float sunAlignH = saturate(dot(normalize(float2(skyRayDir.x, skyRayDir.z)),
                                   normalize(float2(normalizedSunDir.x, normalizedSunDir.z))));

    // 夕焼けの発生条件を厳しくする
    // 太陽が低い時（sunHeightが0付近） かつ 太陽のすぐ近く（sunAlignHを4乗して絞る）のみ
    float sunsetIntensity = smoothstep(0.3, 0.0, abs(sunHeight)) * pow(sunAlignH, 4.0);

    // 最終的な水平線の色
    float3 horizonColor = lerp(baseHorizonColor, horizonSunset, sunsetIntensity);

    // 太陽から離れてもベースの青いグローはしっかり出すために強さを調整
    float horizonGlow = saturate(horizonBand * 1.5);
    skyColor = lerp(skyColor, horizonColor, horizonGlow);

    // *雲の交差判定* //
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

    // *レイマーチング* //
    if (hitClouds)
    {
        tStart = max(tStart, 0);
        float effectiveEnd = min(tStart + 20000.0, tEnd);

        float minStep = 100.0;
        float maxStep = 600.0;

        float3 pos = cameraPos + rayDir * tStart;
        float randomJitter = frac(sin(dot(input.uv, float2(127.1, 311.7)))
                           * 43758.5 + time * 0.1);
        pos += rayDir * (minStep * randomJitter * 0.5);

        float t = tStart;

        for (int i = 0; i < 48; i++)
        {
            if (t >= effectiveEnd)
                break;

            float d = CloudDensity(pos);

            // 3段階ステップ
            float stepLen = (d < 0.01) ? maxStep : minStep;
            float opticalDepth = d * stepLen * cloudOpacity;

            if (opticalDepth > 0.001)
            {
                // HG位相関数
                float cosTheta = dot(rayDir, normalizedSunDir);
                float g = 0.7;
                float phase = (1.0 - g * g)
                                 / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5)
                                 / (4.0 * 3.14159);
                float phaseBlend = lerp(phase, 1.0, 0.3);

                float powderEffect = 1.0 - exp(-opticalDepth * 2.0);
                float lightScattering = exp(-d * 50.0) * powderEffect * phaseBlend;

                // 夕焼け色
                float sunsetSpread = smoothstep(-1.0, 1.0, cosTheta);
                float3 wideSunsetColor = float3(1.0, 0.35, 0.1) * sunsetSpread * sunsetTime;

                // 太陽色（昼→夕焼け→夜でブレンド）
                float3 daySunColor = float3(1.0, 0.92, 0.85);
                float3 sunsetSunColor = float3(1.0, 0.45, 0.05);
                float3 nightSunColor = float3(0.08, 0.10, 0.18); // 月明かり
                float3 currentSunColor = lerp(daySunColor, sunsetSunColor, sunsetTime);
                currentSunColor = lerp(nightSunColor, currentSunColor, dayFactor);

                // 太陽光強度（夜は大幅減）
                float sunIntensity = lerp(0.2, 7.0, dayFactor);
                float3 sunIllumination = currentSunColor * sunIntensity * lightScattering;

                // 環境光（夜は暗い紺）
                float3 nightAmbient = float3(0.015, 0.02, 0.05);
                float skyLuma = dot(cloudAmbientSkyColor, float3(0.299, 0.587, 0.114));
                float3 desaturatedSky = lerp(float3(skyLuma, skyLuma, skyLuma),
                                             cloudAmbientSkyColor, 0.2);
                float3 dayAmbient = (desaturatedSky * 0.1) + (wideSunsetColor * 0.4);
                float3 cloudAmbient = lerp(nightAmbient, dayAmbient, dayFactor);

                // ライト関数
                float3 light = float3(0, 0, 0);
                if (isRialLight)
                    light = RialLightCloud(pos);
                if (isAnimeLight)
                    light = AnimeLightCloud(pos);

                // ライト関数の結果も昼夜でスケール
                light = light * lerp(0.05, 1.0, dayFactor);

                light += sunIllumination + cloudAmbient;

                if (isStorm)
                {
                    float3 lightning = CalculateLightning(pos, time, cameraPos, cloudBottom);
                    // 雲の内部ほど少し光が拡散・減衰するような見栄えにする
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

    // *最終合成* //
    float3 finalColor = color;
    float outAlpha = 1.0 - transmittance; // 雲自体の不透明度

    // ★ 深度が1.0(オブジェクトがない空)の場合のみ、背景の空を加算して不透明にする
    if (depth >= 1.0)
    {
        finalColor += skyColor * transmittance;
        outAlpha = 1.0;
    }

    // 露出やコントラストなどのポストプロセス（既存のまま）
    float exposure = lerp(0.6, 1.5, dayFactor);
    finalColor = 1.0 - exp(-finalColor * exposure);
    finalColor = max(finalColor, 0.0);
    
    float contrast = 1.5;
    finalColor = pow(finalColor, float3(contrast, contrast, contrast));
    finalColor = finalColor * finalColor * (3.0 - 2.0 * finalColor);

    float saturation = lerp(0.7, 1.2, dayFactor);
    float luminance = dot(finalColor, float3(0.299, 0.587, 0.114));
    finalColor = lerp(float3(luminance, luminance, luminance), finalColor, saturation);

    // *太陽ディスク（HDR）の描画* //
    // ★ オブジェクトの裏に太陽が透けないよう、背景(depth >= 1.0)の時のみ描画
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

    // ★ C++側のアルファブレンド(SRC_ALPHA)で正しく合成させるための除算
    // 雲がオブジェクトの上に描かれる際、二重に暗くなるのを防ぎます
    if (depth < 1.0 && outAlpha > 0.001)
    {
        finalColor /= outAlpha;
    }

    PSOutput output;
    
    // *モーションブラー* //
    if (isMotionBlur)
    {
        // 前フレームのクリップ空間座標を計算（背景は無限遠として扱うため、算出したworld座標をそのまま使う）
        float4 prevClip = mul(prevViewProj, float4(world.xyz, 1.0));
        prevClip.xyz /= prevClip.w;

        // 前フレームのUV座標に変換
        float2 prevUV = prevClip.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

        // Velocity = 現在のUV - 過去のUV
        float2 velocity = input.uv - prevUV;
        
        // 倍率
        float cloudBlurStrength = 5.0;
        velocity *= (1.0 - transmittance) * cloudBlurStrength;
        // 出力構造体にセットして返す
        output.Velocity = velocity;
    }
    else
    {
        output.Velocity = float2(0, 0);

    }
    output.Color = float4(finalColor, outAlpha);

    return output;
}