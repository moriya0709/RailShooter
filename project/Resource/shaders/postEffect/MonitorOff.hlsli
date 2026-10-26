void ApplyMonitorOff(inout float2 uv, out float mask, out float3 flash, float progress, float2 center)
{
    float2 originalCenteredUV = uv - center;

    // 1. 縦に潰れる (進行度 0.0 ～ 0.3)
    float scaleY = 1.0f - smoothstep(0.0f, 0.3f, progress);
    
    // 2. 横に潰れる (進行度 0.3 ～ 0.6)
    float scaleX = 1.0f - smoothstep(0.3f, 0.6f, progress);
    
    // 3. 残った光の点がフェードアウト (進行度 0.6 ～ 0.9)
    float fade = 1.0f - smoothstep(0.6f, 0.9f, progress);

    // --- UVのスケール処理 ---
    float2 centeredUV = uv - center;
    centeredUV.x /= max(scaleX, 0.0001f);
    centeredUV.y /= max(scaleY, 0.0001f);
    uv = centeredUV + center;

    // --- マスク処理（圧縮された画面の外側を真っ黒にする） ---
    float maskX = step(0.0f, uv.x) * step(uv.x, 1.0f);
    float maskY = step(0.0f, uv.y) * step(uv.y, 1.0f);
    mask = maskX * maskY * fade;

    // --- 内側に凹んだひし形（四芒星）の光処理 ---
    float flashTiming = smoothstep(0.3f, 0.4f, progress) * (1.0f - smoothstep(0.5f, 0.8f, progress));

    // 絶対値を取って四象限を対称にする
    float2 absUV = abs(originalCenteredUV);
    
    // sqrtを使うことで、辺が中央に向かって曲線になる距離を計算
    float2 starScale = float2(1.0f, 3.0f);
    float2 scaledUV = absUV * starScale;
    float starDist = sqrt(scaledUV.x) + sqrt(scaledUV.y);
    
    // 距離をもとに星の形を描画（1.0f が星の大きさ）
    float starShape = smoothstep(0.8f, 0.0f, starDist);
    // 累乗して、光の芯をより細く・鋭くする
    starShape = pow(starShape, 2.5f);
    
    // 中心部分の強い輝き
    float centerGlow = smoothstep(0.05f, 0.0f, length(originalCenteredUV));

    // 星の光と中心の光を合成して発光させる
    float crossGlow = (starShape + centerGlow) * flashTiming * 15.0f;
    
    // 画面が潰れる瞬間の全体発光
    float baseFlash = smoothstep(0.1f, 0.3f, progress) * (1.0f - smoothstep(0.3f, 0.6f, progress)) * 2.0f;

    // マスクと合成
    flash = float3(1.0f, 1.0f, 1.0f) * (crossGlow + (baseFlash * mask));
}