// めくり制御用の定数バッファ
cbuffer PageCurlData : register(b0)
{
    float curlX; // 曲がり始めるX座標
    float curlRadius; // 曲がる半径 R
    float baseZ; // このページ全体の基本Z座標
    
    int isPageTurnR;
    int isPageTurnL;
    
    float3 padding;
};

// 座標変換用の定数バッファ
cbuffer TransformMatrix : register(b1)
{
    matrix WorldViewProjectionMatrix;
};

// Hull Shaderからの出力と構造体を合わせる
struct HS_CONSTANT_DATA_OUTPUT
{
    float EdgeTessFactor[4] : SV_TessFactor;
    float InsideTessFactor[2] : SV_InsideTessFactor;
};

struct HS_CONTROL_POINT_OUTPUT
{
    float3 pos : WORLDPOS;
    float2 uv : TEXCOORD;
};

struct DS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float shadow : TEXCOORD1; // ★追加：ピクセルシェーダに渡す影の強さ
};

[domain("quad")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT input, float2 UV : SV_DomainLocation, const OutputPatch<HS_CONTROL_POINT_OUTPUT, 4> patch)
{
    DS_OUTPUT output;

    // バイリニア補間
    float3 topPos = lerp(patch[0].pos, patch[1].pos, UV.x);
    float3 bottomPos = lerp(patch[3].pos, patch[2].pos, UV.x);
    float3 localPos = lerp(topPos, bottomPos, UV.y);
    
    float2 topTexUV = lerp(patch[0].uv, patch[1].uv, UV.x);
    float2 bottomTexUV = lerp(patch[3].uv, patch[2].uv, UV.x);
    output.uv = lerp(topTexUV, bottomTexUV, UV.y);

    // めくる方向と進行度の計算
    float PI = 3.141592f;
    
    // 計算には絶対値(abs)を使うことで、マイナス方向の時も手前に曲げる
    float absCurlX = abs(curlX);
    float side = 0.0f;
    bool isMovingSide = false;
    float foldX = 0.5f;
    
    if (isPageTurnR != 0)
    {
        // 右ページをめくる：中央より右側(x > 0.5)を、左方向(side=1.0)へ曲げる
        isMovingSide = (localPos.x > foldX);
        side = 1.0f; 
    }
    else if (isPageTurnL != 0)
    {
        // 左ページをめくる：中央より左側(x < 0.5)を、右方向(side=-1.0)へ曲げる
        isMovingSide = (localPos.x < foldX);
        side = -1.0f;
    }

    // 進行度の計算（absCurlX を使う）
    float progress = clamp(absCurlX / PI, 0.0f, 1.0f);
    float delayAmount = 0.1f;
    float delayOffset = UV.y * delayAmount;
    float localProgress = clamp(progress * (1.0f + delayAmount) - delayOffset, 0.0f, 1.0f);
    
    // この localSafeAngle は常に 0 ～ PI (正の値) になる
    float localSafeAngle = localProgress * PI;
    

    // *影の計算* //
    float distToFold = abs(UV.x - foldX);

    // 折り目の影
    float creaseShadowStrength = 1.0f * saturate(sin(localSafeAngle)); // ★変更
    float creaseShadow = creaseShadowStrength * (1.0f - smoothstep(0.0f, 0.01f, distToFold));
    creaseShadow = pow(creaseShadow, 2.0f);

    // 重なる部分の影
    float overlapShadow = 0.0f;
    bool isShadowSide = (side > 0.0f) ? (UV.x <= foldX) : (UV.x >= foldX);
    if (isShadowSide)
    {
        float closeFactor = smoothstep(0.8f, PI, localSafeAngle); // ★変更
        float overlapStrength = 1.0f * closeFactor;
        float overlapRange = 0.8f * closeFactor + 0.001f;
        overlapShadow = overlapStrength * (1.0f - smoothstep(0.0f, overlapRange, distToFold));
    }

    output.shadow = 1.0f - saturate(max(creaseShadow, overlapShadow));
    

    // *ページを曲げる計算* //
    localPos.z = baseZ;

    if (isMovingSide)
    {
        float d = abs(localPos.x - foldX);
        
        // localSafeAngle は常に正なので、sin(localSafeAngle) も常に正になる
        float currentBend = sin(localSafeAngle) * curlRadius;
        float localAngle = localSafeAngle - (currentBend * d);
        
        // 1基本となる「曲がり」の変位（折り目からの移動量）を計算
        float offsetX = side * d * cos(localAngle);
        float offsetZ = -(d * sin(localAngle)); // マイナスなので手前に行く
        
        // *ポップアウト・エフェクト* //
        // 手前に来るほど offsetZ はマイナスに大きくなるため、絶対値を取るか -offsetZ で深度を測る
        float depth_from_base = saturate(-offsetZ);
        
        // 2倍に強調し、拡大率(scaleFactor)を決定
        float scaleFactor = 1.0f + 0.5f * saturate(depth_from_base * 2.0f);

        // 変位(X, Z)の両方にスケールを掛けて、カーブを歪ませずに拡大する
        localPos.x = foldX + offsetX * scaleFactor;
        localPos.z = baseZ + offsetZ * scaleFactor - 0.005f;

        // Y座標のスケーリング（ページ中央を基準に）
        float y_center = 0.5f;
        localPos.y = y_center + (localPos.y - y_center) * scaleFactor;
    }
    
    // World -> View -> Projection 変換
    output.position = mul(float4(localPos, 1.0f), WorldViewProjectionMatrix);

    return output;
}