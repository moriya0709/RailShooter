RWTexture3D<float4> cloudTexture : register(u0);

// ハッシュ関数
float3 hash(float3 p)
{
    p = float3(dot(p, float3(127.1, 311.7, 74.7)),
               dot(p, float3(269.5, 183.3, 246.1)),
               dot(p, float3(113.5, 271.9, 124.6)));
    return frac(sin(p) * 43758.5453123);
}

// タイル対応 Perlin ノイズ（自然なモコモコ）
float perlinTiling(float3 p, float period)
{
    float3 i = floor(p);
    float3 f = frac(p);

    // より滑らかな補間 (Quintic curve)
    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

// 勾配(Gradient)を計算して内積をとるマクロ
#define grad(x, y, z) \
        dot(hash(fmod(i + float3(x, y, z) + period * 100.0, period)) * 2.0 - 1.0, f - float3(x, y, z))

    float noiseVal = lerp(
        lerp(lerp(grad(0, 0, 0), grad(1, 0, 0), u.x),
             lerp(grad(0, 1, 0), grad(1, 1, 0), u.x), u.y),
        lerp(lerp(grad(0, 0, 1), grad(1, 0, 1), u.x),
             lerp(grad(0, 1, 1), grad(1, 1, 1), u.x), u.y), u.z);
#undef grad

    // -1.0～1.0 の結果を 0.0～1.0 に直して返す
    return noiseVal * 0.5 + 0.5;
}

// タイル対応 FBM (Perlinを複数回重ねる)
float fbmTiling(float3 p, float basePeriod)
{
    float f = 0.0, amp = 0.5, period = basePeriod;
    for (int i = 0; i < 4; i++)
    {
        f += amp * perlinTiling(p, period);
        p *= 2.0;
        period *= 2.0;
        amp *= 0.5;
    }
    return f;
}

// タイル対応 Worley ノイズ（細胞のような泡感）
float worleyTiling(float3 p, float period)
{
    float3 ip = floor(p);
    float minDist = 1e9;

    for (int z = -1; z <= 1; z++)
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
            {
                float3 neighbor = ip + float3(x, y, z);

                // 負の数を防ぎつつタイリング周期でラップアラウンド
                float3 wrappedNeighbor = fmod(neighbor + period * 100.0, period);

                // 完全なランダムベクトルを使って特徴点を配置
                float3 featurePoint = neighbor + hash(wrappedNeighbor);

                // 距離計算（タイリングの反対側から回り込んだ距離も考慮）
                float3 diff = abs(p - featurePoint);
                diff.x = diff.x > period * 0.5 ? period - diff.x : diff.x;
                diff.y = diff.y > period * 0.5 ? period - diff.y : diff.y;
                diff.z = diff.z > period * 0.5 ? period - diff.z : diff.z;

                minDist = min(minDist, length(diff));
            }
    return minDist;
}


// メイン関数（テクスチャへの焼き付け）
[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float3 uvw = (float3) DTid / 128.0;

    // Rチャンネル: Base Noise (低周波 FBM + Worley)
    float baseScale = 4.0;
    float b = fbmTiling(uvw * baseScale, baseScale);
    float w = 1.0 - saturate(worleyTiling(uvw * baseScale, baseScale) * 1.8);
    float baseNoise = saturate(b * 0.55 + w * 0.45);

    // Gチャンネル: Detail Noise (高周波 FBM)
    float detailScale = 12.0;
    float detailNoise = fbmTiling(uvw * detailScale, detailScale);

    // Bチャンネル: Coverage Noise (超低周波 ドメインワープ付き)
    float covScale = 2.0;
    float3 warp = float3(
        fbmTiling(uvw * covScale + float3(1.7, 9.2, 3.4), covScale),
        fbmTiling(uvw * covScale + float3(8.3, 2.8, 5.1), covScale),
        fbmTiling(uvw * covScale + float3(4.1, 7.6, 1.9), covScale)
    ) * 0.5;
    
    float3 warpedPos = fmod(uvw * covScale + warp + covScale, covScale);
    float rawCov = fbmTiling(warpedPos, covScale);
    float coverage = saturate((rawCov - 0.35) * 2.5);

    // 結果を書き込む
    cloudTexture[DTid] = float4(baseNoise, detailNoise, coverage, 1.0);
}