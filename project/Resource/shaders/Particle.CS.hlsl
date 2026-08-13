// ParticleUpdate.hlsl
#include "Particle.hlsli"

// 定数バッファ（カメラ情報やデルタタイム、フィールド情報など）
cbuffer CommonData : register(b0)
{
    float gDeltaTime;
    uint gMaxParticles;
    float2 pad1;
    
    float3 gFieldMin; // 加速フィールドのAABB最小値
    float pad2;
    
    float3 gFieldMax; // 加速フィールドのAABB最大値
    float pad3;
    
    float3 gAcceleration; // 加速フィールドの加速度
    uint gUseField; // フィールドを使用するかどうか
};

RWStructuredBuffer<Particle> gParticles : register(u0);
// インデックス管理用のカウンターバッファ
RWStructuredBuffer<uint> gFreeListIndex : register(u1);
// FreeList本体
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gMaxParticles)
        return;

    // 生きていないパーティクルは処理しない
    if (gParticles[index].isActive == 0)
        return;

    // 1. 寿命による消滅判定
    // 現在の時間は gParticles[index].currentTime、寿命は gParticles[index].lifeTime を参照しています[cite: 6]
    if (gParticles[index].currentTime >= gParticles[index].lifeTime)
    {
        gParticles[index].isActive = 0;
        gParticles[index].scale = float3(0.0f, 0.0f, 0.0f); // 描画サイズを0にする[cite: 6]
        
        // --- 追加部分: FreeListへの返却 (Push) ---
        uint freeListIndex;
        // カウンタを1増やし、増やす前の値（次に書き込むべき配列要素のインデックス）を取得する
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        
        // 最大パーティクル数 (gMaxParticles) を超えない範囲で安全に書き込む[cite: 6]
        if (freeListIndex < gMaxParticles)
        {
            gFreeList[freeListIndex] = index; // 自身のインデックスを空きリストに登録
        }
        
        return;
    }

    // 2. 加速フィールド（AABB Collision）
    if (gUseField != 0)
    {
        float3 pos = gParticles[index].translate;
        if (pos.x >= gFieldMin.x && pos.x <= gFieldMax.x &&
            pos.y >= gFieldMin.y && pos.y <= gFieldMax.y &&
            pos.z >= gFieldMin.z && pos.z <= gFieldMax.z)
        {
            gParticles[index].velocity += gAcceleration * gDeltaTime;
        }
    }

    // 3. 移動
    if (gParticles[index].isRandVelocity.x != 0)
    {
        gParticles[index].translate.x += gParticles[index].velocity.x * gDeltaTime;
    }
    if (gParticles[index].isRandVelocity.y != 0)
    {
        gParticles[index].translate.y += gParticles[index].velocity.y * gDeltaTime;
    }
    if (gParticles[index].isRandVelocity.z != 0)
    {
        gParticles[index].translate.z += gParticles[index].velocity.z * gDeltaTime;
    }

    
    // 4. 色変化
    float progress = (gParticles[index].currentTime / gParticles[index].lifeTime) * gParticles[index].colorChangeSpeed;
    progress = saturate(progress); // 0.0f 〜 1.0f にクランプ

    if (gParticles[index].isColorChange.x != 0)
    {
        gParticles[index].color.x = lerp(gParticles[index].color.x, gParticles[index].finalColor.x, progress);
    }
    if (gParticles[index].isColorChange.y != 0)
    {
        gParticles[index].color.y = lerp(gParticles[index].color.y, gParticles[index].finalColor.y, progress);
    }
    if (gParticles[index].isColorChange.z != 0)
    {
        gParticles[index].color.z = lerp(gParticles[index].color.z, gParticles[index].finalColor.z, progress);
    }
    if (gParticles[index].isColorChange.w != 0)
    {
        gParticles[index].color.w = lerp(gParticles[index].color.w, gParticles[index].finalColor.w, progress);
    }

    // 5. サイズ変化
    if (gParticles[index].isScaleChange.x != 0)
    {
        gParticles[index].scale.x = max(0.0f, gParticles[index].scale.x + gParticles[index].scaleAdd);
    }
    if (gParticles[index].isScaleChange.y != 0)
    {
        gParticles[index].scale.y = max(0.0f, gParticles[index].scale.y + gParticles[index].scaleAdd);
    }
    if (gParticles[index].isScaleChange.z != 0)
    {
        gParticles[index].scale.z = max(0.0f, gParticles[index].scale.z + gParticles[index].scaleAdd);
    }

    // 6. UVスクロール
    gParticles[index].uvOffset += gParticles[index].uvScrollSpeed * gDeltaTime;

    // 7. 時間経過
    gParticles[index].currentTime += gDeltaTime;
    
    
}