#include "PostEffect.hlsli"
#include "Inversion.hlsli"
#include "Grayscale.hlsli"
#include "RadialBlur.hlsli"
#include "DistanceFog.hlsli"
#include "HeightFog.hlsli"
#include "DoF.hlsli"
#include "MotionBlur.hlsli"
#include "LensFlare.hlsli"
#include "Bloom.hlsli"
#include "SpeedDistortion.hlsli"
#include "Pinch.hlsli"
#include "ConcentrationLines.hlsli"
#include "Vignette.hlsli"
#include "Outline.hlsli"
#include "BoxFilter.hlsli"
#include "Dissolve.hlsli"
#include "RandomNoise.hlsli"

//* 上記のファイルで何故か日本語が使えない *//

Texture2D<float4> gCurrentTexture : register(t0);
Texture2D<float4> gBloom1Texture : register(t1); // 1/2ぼかし
Texture2D<float4> gBloom2Texture : register(t2); // 1/4ぼかし
Texture2D<float4> gBloom3Texture : register(t3); // 1/8ぼかし
Texture2D<float> gDepthTexture : register(t4); // 深度はt4に移動
Texture2D<float4> gLensFlareTexture : register(t5); // レンズフレア
Texture2D<float2> gVelocityTexture : register(t6); // RGチャンネルのみの想定

SamplerState gSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct EffectData
{
    int isInversion; // 色反転
    int isGrayscale; // グレースケール
    int isRadialBlur; // 放射状ブラー
    int isDistanceFog; // ディスタンスフォグ

    int isDoF; // 被写界深度
    int isHeightFog; // ハイトフォグ
    float intensity; // 全体のエフェクト強度
    float pad0;

    float2 blurCenter; // 放射状ブラーの中心 (0.0〜1.0)
    float blurWidth; // 放射状ブラーの幅
    int blurSamples; // 放射状ブラーのサンプル数

    float3 distanceFogColor; // ディスタンスフォグの色
    float distanceFogStart; // ディスタンスフォグの開始距離

    float distanceFogEnd; // ディスタンスフォグの終了距離
    float zNear; // 深度の線形化に必要なカメラの近クリップ面
    float zFar; // 深度の線形化に必要なカメラの遠クリップ面
    float pad1;

    // *ハイトフォグ* //
    float3 heightFogColor; // 色
    float heightFogTop; // 高さ(最上値)

    float heightFogBottom; // 高さ(最下値)
    float heightFogDensity; // ハイトフォグの密度（高さによる減衰の強さ）
    float2 pad2;

    float4x4 matInverseViewProjection; // ハイトフォグのワールド位置計算に必要な逆行列

    // *DoF* //
    float focusDistance; // ピントの距離
    float focusRange; // ピントの範囲（この距離内は完全にピントが合う）
    float bokehRadius; // ボケの半径（大きいほど大きくボケる）
    float pad3;

   // *ブルーム* //
    float bloomThreshold; // 光らせる明るさの閾値
    float bloomIntensity; // ブルームの強さ
    float bloomBlurRadius; // ブルームのぼかし半径（広がり具合）
    float pad4;
    
   // *レンズフレア* //
    int isLensFlare; // レンズフレアのON/OFF
    int lensFlareGhostCount; // ゴーストの数
    float lensFlareGhostDispersal; // ゴーストの広がり具合
    float lensFlareHaloWidth; // ヘイローの大きさ
    
    int isACES; // ACESトーンマッピングのON/OFF
    float caIntensity; // 色収差の強さ (0.001f とかが綺麗)
    float2 pad5;
    
    // *モーションブラー* //
    int isMotionBlur; // モーションブラーのON/OFF
    int motionBlurSamples; // モーションブラーのサンプル数
    float motionBlurScale; // モーションブラーの強さ（速度に対するスケール）
    float pad6;
    
    // 色収差
    int isFullScreenCA; // 画面全体の色収差ON/OFF
    float fullScreenCAIntensity; // 画面全体の色収差の強さ
    // ビネット
    int isVignette; // ビネットのON/OFF
    float vignetteIntensity; // ビネットの強さ
    
    float3 vignetteColor;   // 色
    float pad7;
    
    
    // スピードディストーション
    int isSpeedDistortion; // スピードディストーションのON/OFF
    float speedDistortionStrength; // 歪みの強さ
    float2 pad8; // アライメント調整用
    
    // 集中線
    int isConcentrationLines; // ON/OFF
    float concentrationLineIntensity; // 線の濃さ
    float2 concentrationLineCenter; // 中心座標 (通常 0.5, 0.5)

    float concentrationLineDensity; // 線の密度（本数）
    float concentrationLineLength; // 線の長さ（中心からの開始距離 0.0〜1.0）
    float concentrationLineSpeed; // アニメーション速度
    float time;
    
    // ピンチエフェクト
    int isPinch; // ON/OFF
    float pinchStrength; // 歪みの強さ（正の値で吸い込み、負の値で膨張）
    float2 pinchCenter; // 歪みの中心 (通常 0.5, 0.5)

    float pinchRadius; // 歪みが影響する半径
    float3 pad9;
    
    // モノクロ
    int isTwoColor; // 二値化
    float threshold; // 白と黒の境界値 (0.0~1.0)
    float contrast; // コントラストの強さ
    float pad10;
    
    // アウトライン
    int isOutline;  // ON/OFF
    float outlineThreshold; // 境界値
    float2 pad11;
    
    float4 outlineColor;    // 色
    
    // 平滑化
    int isBoxFilter;   // ON/OFF
    float3 pad12;
    
    // ディゾルブ
    int isDissolve;     // ON/OFF
    float dissolveThreshold;    // 境界値
    float dissolveEdgeWidth;    // エッジの幅
    float dissolveNoiseScale;   // ノイズの細かさ
    
    float3 dissolveNoiseColor; // ノイズの色
    float pad14;
    float3 dissolveEdgeColor;   // エッジの色
    float pad15;
    
    // ランダムノイズ
    int isRandomNoise;  // ON/OFF
    float3 pad16;
    
};
ConstantBuffer<EffectData> gEffectData : register(b0);

cbuffer RootConstants : register(b1)
{
    int gPassId; // パス番号
};

struct SunAndCloudParam
{
    float4x4 invViewProj;
    float3 cameraPos;
    float time;
    float3 sunDir;
    float cloudCoverage;
    float cloudBottom;
    float cloudTop;
    int isRialLight;
    int isAnimeLight;
    float3 cloudOffset;
    int pad;
};
ConstantBuffer<SunAndCloudParam> gSunCloudData : register(b2);

float4 main(VSOutput input) : SV_TARGET
{
    float4 color = gCurrentTexture.Sample(gSampler, input.uv);
    
    // 高輝度抽出
    if (gPassId == 1)
    {
        return BloomHighBrightness(color, gEffectData.bloomThreshold);
    }

    // ガウスぼかし
    if (gPassId == 2 || gPassId == 3)
    {
        return BloomGaussianBlur(gEffectData.bloomBlurRadius, gPassId, input.uv, gCurrentTexture, gBloom3Texture, gSampler);
    }
    
    // レンズフレア
    if (gPassId == 4)
    {
        return LensFlareMain(gEffectData.lensFlareGhostCount, gEffectData.lensFlareGhostDispersal, gEffectData.lensFlareHaloWidth, gEffectData.caIntensity, input.uv, gCurrentTexture, gBloom3Texture, gSampler);
    }

    // 通常描画＆最終合成
    if (gPassId == 0)
    {
        
        // スピードディストーション
        if (gEffectData.isSpeedDistortion)
        {
            color = SpeedDistortion(color, gEffectData.speedDistortionStrength, input.uv, gCurrentTexture, gSampler);
        }
        
         // 放射状ブラー
        if (gEffectData.isRadialBlur)
        {
            color = RadialBlur(color, gEffectData.blurCenter, gEffectData.blurWidth, gEffectData.blurSamples, input.uv, gCurrentTexture, gSampler);
        }
        
         // DoF
        if (gEffectData.isDoF)
        {
            color = DoF(color, gEffectData.zNear, gEffectData.zFar, gEffectData.focusDistance, gEffectData.focusRange, gEffectData.bokehRadius, input.uv, gCurrentTexture, gDepthTexture, gSampler);
        }
        
        // モーションブラー処理
        if (gEffectData.isMotionBlur)
        {
            color = MotionBlur(color, gEffectData.motionBlurSamples, gEffectData.motionBlurScale, input.uv, gCurrentTexture, gVelocityTexture, gSampler);
        }
        
        // ピンチエフェクト
        if (gEffectData.isPinch)
        {
            color = Pinch(color, gEffectData.pinchStrength, gEffectData.pinchCenter, gEffectData.pinchRadius, input.uv, gCurrentTexture, gSampler);
        }
        
        // 平滑化
        if (gEffectData.isBoxFilter)
        {
            color = BoxFilter5x5(input.uv, gCurrentTexture, gSampler);
        }
        
        // ディゾルブ
        if (gEffectData.isDissolve)
        {
            color = Dissolve(color, gEffectData.dissolveThreshold, gEffectData.dissolveEdgeWidth, gEffectData.dissolveNoiseColor, gEffectData.dissolveEdgeColor, input.uv, gEffectData.dissolveNoiseScale);
        }
        
        // ランダムノイズ
        if (gEffectData.isRandomNoise)
        {
            color = RandomNoise(input.uv, gEffectData.time);
        }
        
        // フルスクリーン色収差
        if (gEffectData.isFullScreenCA)
        {
            float2 toCenter = float2(0.5f, 0.5f) - input.uv;
            // ベースカラーをズレた色で上書き
            color.rgb = SampleWithCA(gCurrentTexture, gSampler, input.uv, toCenter, gEffectData.fullScreenCAIntensity);
        }
        
         // 集中線の実装
        if (gEffectData.isConcentrationLines)
        {
            color.rgb = ConcentrationLines(color, gEffectData.concentrationLineIntensity, gEffectData.concentrationLineCenter, gEffectData.concentrationLineDensity, gEffectData.concentrationLineLength, gEffectData.concentrationLineSpeed, gEffectData.time, input.uv);
        }
        
        // ビネット
        if (gEffectData.isVignette)
        {
            color.rgb = Vignette(color, gEffectData.vignetteIntensity, gEffectData.vignetteColor,input.uv);
        }

        // ディスタンスフォグ
        if (gEffectData.isDistanceFog)
        {
            color.rgb = DistanceFog(color, gEffectData.distanceFogColor, gEffectData.distanceFogStart, gEffectData.distanceFogEnd, gEffectData.zNear, gEffectData.zFar, input.uv, gDepthTexture, gSampler);
        }

        // ハイトフォグ
        if (gEffectData.isHeightFog)
        {
            color.rgb = HeightFog(color, gEffectData.heightFogColor, gEffectData.heightFogTop, gEffectData.heightFogBottom, gEffectData.heightFogDensity, gEffectData.matInverseViewProjection, input.uv, gDepthTexture, gSampler);
        }
        
        // ブルームの最終合成
        color.rgb = BloomComposite(color, gEffectData.bloomIntensity, input.uv, gBloom1Texture, gBloom2Texture, gBloom3Texture, gSampler);
    
        // レンズフレアの最終合成
        if (gEffectData.isLensFlare)
        {
            color.rgb += LensFlareComposite(input.uv, gLensFlareTexture, gSampler);
        }
        
        // モノクロ
        if (gEffectData.isGrayscale)
        {
            color.rgb = Grayscale(color, gEffectData.isTwoColor, gEffectData.threshold, gEffectData.contrast);

        }
        
        // 色反転
        if (gEffectData.isInversion)
        {
            color.rgb = Inversion(color);
        }

        // アウトライン
        if (gEffectData.isOutline)
        {
            color.rgb = Outline(color, gEffectData.outlineThreshold, gEffectData.outlineColor, input.uv, gDepthTexture, gSampler);
        }
        
        color.rgb *= gEffectData.intensity;

        // ACESトーンマッピング
        if (gEffectData.isACES)
        {
            color.rgb = ACESFitted(color.rgb);
        }
        else
        {
            // Reinhard (指数トーンマッピング)
            float exposure = 1.0f;
            color.rgb = 1.0f - exp(-color.rgb * exposure);
        }
    }

    return color;
}