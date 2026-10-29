// EmitParticle.CS.hlsl
#include "Particle.hlsli"

// CPUから渡される射出パラメータ（定数バッファ）
cbuffer EmitData : register(b0)
{
    float4 color;
    float2 uvScale; // UVスケール
    float2 uvOffset; // UVオフセット

	// --- 更新計算用パラメータ ---
    float3 translate;
    float pad1;
    float3 scale;
    float pad2;
    float3 rotate;
    float pad3;
    int3 isRandPosition; // ランダムな座標にするかどうか
    float pad5;
    int3 isRandScale; // ランダムなスケールにするかどうか
    float pad6;
    int3 isRandRotate; // ランダムな回転にするかどうか
    float pad7;
    int3 isRandVelocity; // ランダムに動かすかどうか
    float pad8;
    int3 isScaleChange; // スケール変更するかどうか
    float pad9;

    int4 isColorChange; // 色変更するかどうか
    float4 finalColor;

    float lifeTime;
    float currentTime;
    float colorChangeSpeed;
    float scaleAdd; // スケール変更量

    float emissive; // エミッシブ
    float2 uvScrollSpeed; // UVスクロール速度
    int pad10;

    int useNoise; // 0:通常 1:ノイズテクスチャ 2:両方
    float3 burnColor; // ふちの色

    int count; //!< 発生数
    float frequency; //!< 発生頻度
    float frequencyTime; //!< 頻度用時刻
    int pad11;
    
    float2 randPosition;
    float2 randScale;
    float2 randRotate;
    float2 randVelocity;
    
    float2 randLifeTime;
    float2 pad12;
    
};

// パーティクル本体のバッファ
RWStructuredBuffer<Particle> gParticles : register(u0);
// インデックス管理用のカウンターバッファ (サイズ: 1のuint)
RWStructuredBuffer<uint> gFreeListIndex : register(u1);
// FreeList本体(空いているパーティクルのインデックス配列)
RWStructuredBuffer<uint> gFreeList : register(u2);

// 簡易的なGPU乱数生成関数（シード値にスレッドID等を利用）
float Random(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(64, 1, 1)] // 射出数に合わせてスレッドを起動
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint id = DTid.x;
    if (id >= count)
        return;


    // 空きリストから 1 枠を安全に予約する。
    // InterlockedAdd で 0 から減算して後で戻す方式だと、同時に走る別スレッドが
    // uint のアンダーフロー値を空き数として読み、範囲外の FreeList を参照してしまう。
    // CAS で「空きがある場合だけ」デクリメントすることで、満員時は安全に射出を捨てる。
    uint expectedFreeCount;
    InterlockedAdd(gFreeListIndex[0], 0, expectedFreeCount);

    uint particleIndex;
    while (expectedFreeCount != 0)
    {
        uint observedFreeCount;
        InterlockedCompareExchange(
            gFreeListIndex[0],
            expectedFreeCount,
            expectedFreeCount - 1,
            observedFreeCount);

        if (observedFreeCount == expectedFreeCount)
        {
            particleIndex = gFreeList[expectedFreeCount - 1];
            break;
        }

        // 他スレッドが先に空き数を更新したので、最新値で再試行する。
        expectedFreeCount = observedFreeCount;
    }

    if (expectedFreeCount == 0)
    {
        return;
    }

    // *ランダム座標* //
    
    // ランダム値の計算（DTid等を利用してシードをばらけさせる）
    float2 randomSeed = float2(particleIndex + currentTime, translate.x + currentTime);
    // 1. まず 0.0 ~ 1.0 の乱数を作る
    float3 rawRandom = float3(Random(randomSeed), Random(randomSeed + 1.0), Random(randomSeed + 2.0));
    // 2. lerpを使って、-10.0 と 10.0 の間で変換する
    float3 minVal = float3(randPosition.x, randPosition.x, randPosition.x);
    float3 maxVal = float3(randPosition.y, randPosition.y, randPosition.y);
    float3 randomPos = lerp(minVal, maxVal, rawRandom);
    
    // *ランダムスケール* //
    
    // ランダム値の計算（DTid等を利用してシードをばらけさせる）
    randomSeed = float2(particleIndex + 10.0f, scale.x);
    // 1. まず 0.0 ~ 1.0 の乱数を作る
    rawRandom = float3(Random(randomSeed), Random(randomSeed + 1.0), Random(randomSeed + 2.0));
    // xとyの間で変換する
    minVal = float3(randScale.x, randScale.x, randScale.x);
    maxVal = float3(randScale.y, randScale.y, randScale.y);
    float3 randomScale = lerp(minVal, maxVal, rawRandom);
    
    // *ランダム回転* //
    
    // ランダム値の計算（DTid等を利用してシードをばらけさせる）
    randomSeed = float2(particleIndex + 20.0f, rotate.x);
    // 1. まず 0.0 ~ 1.0 の乱数を作る
    rawRandom = float3(Random(randomSeed), Random(randomSeed + 1.0), Random(randomSeed + 2.0));
    // xとyの間で変換する
    minVal = float3(randRotate.x, randRotate.x, randRotate.x);
    maxVal = float3(randRotate.y, randRotate.y, randRotate.y);
    float3 randomRotate = lerp(minVal, maxVal, rawRandom);
        
    // *ランダムベロシティ* //
    
    // ランダム値の計算（DTid等を利用してシードをばらけさせる）
    randomSeed = float2(particleIndex + 30.0f, translate.x + 30.0f);
    // 1. まず 0.0 ~ 1.0 の乱数を作る
    rawRandom = float3(Random(randomSeed), Random(randomSeed + 1.0), Random(randomSeed + 2.0));
    // xとyの間で変換する
    minVal = float3(randVelocity.x, randVelocity.x, randVelocity.x);
    maxVal = float3(randVelocity.y, randVelocity.y, randVelocity.y);
    float3 randomVelocity = lerp(minVal, maxVal, rawRandom);
    
    // *ランダム寿命* //
    
    // ランダム値の計算（DTid等を利用してシードをばらけさせる）
    randomSeed = float2(particleIndex + 40.0f, lifeTime);
    // 1. まず 0.0 ~ 1.0 の乱数を作る
    float rawRandomTime = Random(randomSeed);
    // xとyの間で変換する
    float minTime = randLifeTime.x;
    float maxTime = randLifeTime.y;
    float randomLifeTime = lerp(minTime, maxTime, rawRandomTime);

    // パーティクルの要素
    gParticles[particleIndex].isActive = 1;
    gParticles[particleIndex].currentTime = currentTime;
    gParticles[particleIndex].lifeTime = randomLifeTime;
    gParticles[particleIndex].velocity = randomVelocity;
    gParticles[particleIndex].color = color;
	// 色補間は常に発生時の色を始点にする。
    gParticles[particleIndex].startColor = color;
    gParticles[particleIndex].uvScale = uvScale; // UVスケール
    gParticles[particleIndex].uvOffset = uvOffset; // UVオフセット
    
    // 座標
    // ランダム出現はエミッター（GameObject）の座標からの差分として扱う。
    // 以前はランダム値をワールド座標としてそのまま代入していたため、
    // GameObject が遠い位置にあっても原点付近に発生していた。
    gParticles[particleIndex].translate = translate;
    if (isRandPosition.x)
        gParticles[particleIndex].translate.x += randomPos.x;
    if (isRandPosition.y)
        gParticles[particleIndex].translate.y += randomPos.y;
    if (isRandPosition.z)
        gParticles[particleIndex].translate.z += randomPos.z;
    
    // スケール
    if (isRandScale.x)
        gParticles[particleIndex].scale.x = randomScale.x;
    else
        gParticles[particleIndex].scale.x = scale.x;
    if (isRandScale.y)
        gParticles[particleIndex].scale.y = randomScale.y;
    else
        gParticles[particleIndex].scale.y = scale.y;
    if (isRandScale.z)
        gParticles[particleIndex].scale.z = randomScale.z;
    else
        gParticles[particleIndex].scale.z = scale.z;
    
    // 回転
    if (isRandRotate.x)
        gParticles[particleIndex].rotate.x = randomRotate.x;
    else
        gParticles[particleIndex].rotate.x = rotate.x;
    if (isRandRotate.y)
        gParticles[particleIndex].rotate.y = randomRotate.y;
    else
        gParticles[particleIndex].rotate.y = rotate.y;
    if (isRandRotate.z)
        gParticles[particleIndex].rotate.z = randomRotate.z;
    else
        gParticles[particleIndex].rotate.z = rotate.z;
    
    gParticles[particleIndex].isRandVelocity = isRandVelocity; // ランダムに動かすかどうか
    gParticles[particleIndex].isScaleChange = isScaleChange; // スケール変更するかどうか
    gParticles[particleIndex].isColorChange = isColorChange; // 色変更するかどうか
    gParticles[particleIndex].finalColor = finalColor;
    gParticles[particleIndex].colorChangeSpeed = colorChangeSpeed;
    gParticles[particleIndex].scaleAdd = scaleAdd; // スケール変更量
    gParticles[particleIndex].emissive = emissive; // エミッシブ
    gParticles[particleIndex].uvScrollSpeed = uvScrollSpeed; // UVスクロール速度
    gParticles[particleIndex].useNoise = useNoise; // 0:通常 1:ノイズテクスチャ 2:両方
    gParticles[particleIndex].burnColor = burnColor; // ふちの色

}
