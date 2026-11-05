#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <dxcapi.h>
#include <memory>

#include "Calc.h"
#include "RayMarching.h"

class DirectXCommon;
class WindowAPI;
class SrvManager;
class Camera;

struct RenderTarget {
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{};
};

// C++側の構造体 (PostEffect.h など)
struct EffectData {
	int isInversion; // 色反転
	int isGrayscale; // グレースケール
	int isRadialBlur; // 放射状ブラー
	int isDistanceFog; // ディスタンスフォグ

	int isDoF; // 被写界深度
	int isHeightFog; // ハイトフォグ
	float intensity; // 全体のエフェクト強度
	float pad0;

	Vector2 blurCenter; // 放射状ブラーの中心 (0.0〜1.0)
	float blurWidth; // 放射状ブラーの幅
	int blurSamples; // 放射状ブラーのサンプル数

	Vector3 distanceFogColor; // ディスタンスフォグの色
	float distanceFogStart; // ディスタンスフォグの開始距離

	float distanceFogEnd; // ディスタンスフォグの終了距離
	float zNear; // 深度の線形化に必要なカメラの近クリップ面
	float zFar; // 深度の線形化に必要なカメラの遠クリップ面
	float pad1;

	// *ハイトフォグ* //
	Vector3 heightFogColor; // 色
	float heightFogTop; // 高さ(最上値)

	float heightFogBottom; // 高さ(最下値)
	float heightFogDensity; // ハイトフォグの密度（高さによる減衰の強さ）
	Vector2 pad2;

	Matrix4x4 matInverseViewProjection; // ハイトフォグのワールド位置計算に必要な逆行列

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
	Vector2 pad5;

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

	Vector3 vignetteColor;   // 色
	float pad7;


	// スピードディストーション
	int isSpeedDistortion; // スピードディストーションのON/OFF
	float speedDistortionStrength; // 歪みの強さ
	Vector2 pad8; // アライメント調整用

	// 集中線
	int isConcentrationLines; // ON/OFF
	float concentrationLineIntensity; // 線の濃さ
	Vector2 concentrationLineCenter; // 中心座標 (通常 0.5, 0.5)

	float concentrationLineDensity; // 線の密度（本数）
	float concentrationLineLength; // 線の長さ（中心からの開始距離 0.0〜1.0）
	float concentrationLineSpeed; // アニメーション速度
	float time;

	// ピンチエフェクト
	int isPinch; // ON/OFF
	float pinchStrength; // 歪みの強さ（正の値で吸い込み、負の値で膨張）
	Vector2 pinchCenter; // 歪みの中心 (通常 0.5, 0.5)

	float pinchRadius; // 歪みが影響する半径
	Vector3 pad9;

	// モノクロ
	int isTwoColor; // 二値化
	float threshold; // 白と黒の境界値 (0.0~1.0)
	float contrast; // コントラストの強さ
	float grayscaleProgress; // 白黒化の進行度 (0.0=カラー, 1.0=完全な白黒)

	// アウトライン
	int isOutline;  // ON/OFF
	float outlineThreshold; // 境界値
	Vector2 pad11;

	Vector4 outlineColor;    // 色

	// 平滑化
	int isBoxFilter;   // ON/OFF
	Vector3 pad12;

	// ディゾルブ
	int isDissolve;     // ON/OFF
	float dissolveThreshold;    // 境界値
	float dissolveEdgeWidth;    // エッジの幅
	float dissolveNoiseScale;   // ノイズの細かさ

	Vector3 dissolveNoiseColor; // ノイズの色
	float pad14;
	Vector3 dissolveEdgeColor;   // エッジの色
	float pad15;

	// ランダムノイズ
	int isRandomNoise;  // ON/OFF
	Vector3 pad16;

	// 目を閉じるエフェクト (アイブリンク)
	int isEyeBlink; // ON/OFF
	float eyeBlinkProgress; // 閉じる進行度 (0.0 = 全開, 1.0 = 完全に閉じる)
	Vector2 pad17;

	// モニターオフ (ブラウン管)
	int isMonitorOff; // ON/OFF
	float monitorOffProgress; // 進行度 (0.0 = 通常, 1.0 = 完全に消える)
	Vector2 monitorOffCenter; // 消える最終地点 (X, Y 共に 0.0 ～ 1.0。中央は 0.5, 0.5)

	// グリッチノイズ
	int isGlitch; // ON/OFF
	float glitchIntensity; // 歪みと色ズレの強さ (0.0 ～ 1.0)
	float glitchWaveSpeed;      // アニメーションの波の速さ (目安: 5.0f 程度)
	float glitchWaveThreshold;  // エフェクトが止まる境界値 (0.0fにすると常に止まらない)
	
};

// 各パスのレンダーターゲットとSRVインデックスをまとめる構造体
struct BloomBuffer {
	RenderTarget lumRenderTarget;
	uint32_t lumSrvIndex;

	RenderTarget blurRenderTarget[2];
	uint32_t blurSrvIndex[2];
};

class PostEffect {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, WindowAPI* windowAPI, SrvManager* srvManager);
	// 更新
	void Update(Camera* camera);
	void DamageEffectUpdate();
	// 描画
	void Draw();

	// 描画前処理
	void PreDraw();
	// 2Dなど、HDRシーンカラーだけへ描画するための設定
	void SetSceneColorRenderTarget();
	// 描画後処理
	void PostDraw();
	// 通常シーンの深度を利用して、ポストエフェクト対象だけをマスクへ描画する。
	void PreDrawObjectMask();
	void PostDrawObjectMask();
	// 最終合成先を指定する。Editor の Game ウィンドウにだけゲーム映像を表示するために使用する。
	void SetOutputViewport(float left, float top, float width, float height);

	// 反転
	void SetInversion(bool isInversion) { effectData->isInversion = isInversion; }
	// グレースケール
	void SetGrayscale(bool isGrayscale) { effectData->isGrayscale = isGrayscale; }
	// 放射線ブラー
	void SetRadialBlur(bool isRadialBlur) { effectData->isRadialBlur = isRadialBlur; }
	void SetBlurCenter(const Vector2& center) { effectData->blurCenter = center; }
	void SetBlurWidth(float width) { effectData->blurWidth = width; }
	void SetBlurSamples(int samples) { effectData->blurSamples = samples; }

	// ディスタンスフォグ
	void SetDistanceFog(bool isFog) { effectData->isDistanceFog = isFog; }
	void SetDistanceFogColor(const Vector3& color) { effectData->distanceFogColor = color; }
	void SetDistanceFogStart(float start) { effectData->distanceFogStart = start; }
	void SetDistanceFogEnd(float end) { effectData->distanceFogEnd = end; }

	// ハイトフォグ
	void SetHeightFog(bool isFog) { effectData->isHeightFog = isFog; }
	void SetHeightFogColor(const Vector3& color) { effectData->heightFogColor = color; }
	void SetHeightFogTop(float top) { effectData->heightFogTop = top; }
	void SetHeightFogBottom(float bottom) { effectData->heightFogBottom = bottom; }
	void SetHeightFogDensity(float density) { effectData->heightFogDensity = density; }
	void SetInverseViewProjectionMatrix(const Matrix4x4& mat) { effectData->matInverseViewProjection = mat; }
	void HightFogUpdate(Camera* camera); // カメラの位置からハイトフォグ用の逆行列を計算してセットする関数

	// DOF
	void SetDOF(bool isDoF) { effectData->isDoF = isDoF; }
	void SetFocusDistance(float distance) { effectData->focusDistance = distance; }
	void SetFocusRange(float range) { effectData->focusRange = range; }
	void SetBokehRadius(float radius) { effectData->bokehRadius = radius; }

	// ブルーム
	void SetBloomThreshold(float bloomThreshold) { effectData->bloomThreshold = bloomThreshold; }
	void SetBloomIntensity(float bloomIntensity) { effectData->bloomIntensity = bloomIntensity; }
	void SetBloomBlurRadius(float bloomBlurRadius) { effectData->bloomBlurRadius = bloomBlurRadius; }
	// レンズフレア
	void SetLensFlare(bool isLensFlare) { effectData->isLensFlare = isLensFlare; }
	void SetLensFlareGhostCount(int count) { effectData->lensFlareGhostCount = count; }
	void SetLensFlareGhostDispersal(float dispersal) { effectData->lensFlareGhostDispersal = dispersal; }
	void SetLensFlareHaloWidth(float width) { effectData->lensFlareHaloWidth = width; }
	void SetCAIntensity(float intensity) { effectData->caIntensity = intensity; } // 色収差
	void SetIsACES(bool isACES) { effectData->isACES = isACES; } // ACES
	// モーションブラー
	void SetMotionBlur(bool isMotionBlur) { effectData->isMotionBlur = isMotionBlur; }
	void SetMotionBlurSamples(int motionBlurSamples) { effectData->motionBlurSamples = motionBlurSamples; }
	void SetMotionBlurScale(float motionBlurScale) { effectData->motionBlurScale = motionBlurScale; }
	// 色収差
	void SetFullScreenCA(bool isFullScreenCA) { effectData->isFullScreenCA = isFullScreenCA; }
	void SetFullScreenCAIntensity(float intensity) { effectData->fullScreenCAIntensity = intensity; }
	// ビネット
	void SetVignette(bool isVignette) { effectData->isVignette = isVignette; }
	void SetVignetteIntensity(float intensity) { effectData->vignetteIntensity = intensity; }
	void SetVignetteColor(const Vector3& color) { effectData->vignetteColor = color; }
	// スピードディストーション
	void SetSpeedDistortion(bool isSpeedDistortion) { effectData->isSpeedDistortion = isSpeedDistortion; }
	void SetSpeedDistortionStrength(float strength) { effectData->speedDistortionStrength = strength; }
	// 集中線
	void SetConcentrationLines(bool isConcentrationLines) { effectData->isConcentrationLines = isConcentrationLines; }
	void SetConcentrationLineIntensity(float intensity) { effectData->concentrationLineIntensity = intensity; }
	void SetConcentrationLineCenter(const Vector2& center) { effectData->concentrationLineCenter = center; }
	void SetConcentrationLineDensity(float density) { effectData->concentrationLineDensity = density; }
	void SetConcentrationLineLength(float length) { effectData->concentrationLineLength = length; }
	void SetConcentrationLineSpeed(float speed) { effectData->concentrationLineSpeed = speed; }
	// ピンチエフェクト
	void SetPinch(bool isPinch) { effectData->isPinch = isPinch; }
	void SetPinchStrength(float strength) { effectData->pinchStrength = strength; }
	void SetPinchCenter(const Vector2& center) { effectData->pinchCenter = center; }
	void SetPinchRadius(float radius) { effectData->pinchRadius = radius; }
	// モノクロ
	void SetTwoColor(bool isTwoColor) { effectData->isTwoColor = isTwoColor; }
	void SetThreshold(float threshold) { effectData->threshold = threshold; }
	void SetContrast(float contrast) { effectData->contrast = contrast; }
	void SetGrayscaleProgress(float progress) { effectData->grayscaleProgress = progress; } // 白黒化の進行度 (0.0=カラー, 1.0=完全な白黒)
	// アウトライン
	void SetOutline(bool isOutline) { effectData->isOutline = isOutline; }
	void SetOutlineThreshold(float outlineThreshold) { effectData->outlineThreshold = outlineThreshold; }
	void SetOutlineColor(const Vector4& color) { effectData->outlineColor = color; }
	// 平滑化
	void SetBoxFilter(bool isBoxFilter) { effectData->isBoxFilter = isBoxFilter; }
	// ディゾルブ
	void SetDissolve(bool isDissolve) { effectData->isDissolve = isDissolve; }
	void SetDissolveThreshold(float dissolveThreshold) { effectData->dissolveThreshold = dissolveThreshold; }
	void SetDissolveEdgeWidth(float dissolveEdgeWidth) { effectData->dissolveEdgeWidth = dissolveEdgeWidth; }
	void SetDissolveNoiseScale(float dissolveNoiseScale) { effectData->dissolveNoiseScale = dissolveNoiseScale; }
	void SetDissolveNoiseColor(const Vector3& color) { effectData->dissolveNoiseColor = color; }
	void SetDissolveEdgeColor(const Vector3& color) { effectData->dissolveEdgeColor = color; }
	// ランダムノイズ
	void SetRandomNoise(bool isRandomNoise) { effectData->isRandomNoise = isRandomNoise; }
	// アイブリンク
	void SetEyeBlink(bool isEyeBlink) { effectData->isEyeBlink = isEyeBlink; }
	void SetEyeBlinkProgress(float progress) { effectData->eyeBlinkProgress = progress; }
	// モニターオフ (ブラウン管)
	void SetMonitorOff(bool isMonitorOff) { effectData->isMonitorOff = isMonitorOff; }
	void SetMonitorOffProgress(float progress) { effectData->monitorOffProgress = progress; }
	void SetMonitorOffCenter(const Vector2& center) { effectData->monitorOffCenter = center; }
	// グリッチノイズ
	void SetGlitch(bool isGlitch) { effectData->isGlitch = isGlitch; }
	void SetGlitchIntensity(float intensity) { effectData->glitchIntensity = intensity; }
	void SetGlitchWaveSpeed(float speed) { effectData->glitchWaveSpeed = speed; }
	void SetGlitchWaveThreshold(float threshold) { effectData->glitchWaveThreshold = threshold; }

	// エフェクトの強さ
	void SetIntensity(float intensity) { effectData->intensity = intensity; }


	// ダメージエフェクト
	void SetDamageEffectRatio(float ratio) { damageEffectRatio_ = ratio; }

	// getter
	float GetLensFlareGhostDispersal() { return effectData->lensFlareGhostDispersal; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(uint32_t index) { return rtvHandles[index]; }
	uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

	// シングルトンインスタンスの取得
	static PostEffect* GetInstance();

	// 深度バッファを指定の状態に遷移
	void TransitionDepthBuffer(D3D12_RESOURCE_STATES newState);

private:
	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateFinal;

	// レンダーターゲット
	RenderTarget renderTarget_;
	RenderTarget lumRenderTarget_;     // 高輝度抽出用
	RenderTarget blurRenderTarget_[2]; // ぼかし用（Ping-Pong処理用）
	RenderTarget lensFlareRenderTarget_; // レンズフレア
	RenderTarget velocityRenderTarget_; // ベロシティ
	RenderTarget objectMaskRenderTarget_; // オブジェクト単位ポストエフェクト用マスク
	// ビューポート
	D3D12_VIEWPORT viewport_;
	// シザー矩形
	D3D12_RECT scissorRect_;
	// 最終合成（バックバッファ）専用の表示領域。シーン描画用の viewport_ とは分離する。
	D3D12_VIEWPORT outputViewport_;
	D3D12_RECT outputScissorRect_;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// クリアカラー
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// *エフェクト切り換え用* //
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> effectResource;
	// エフェクトデータ
	EffectData* effectData = nullptr;

	// index
	uint32_t srvIndex_;
	// PostEffect.h の private メンバ変数などに以下を追加
	uint32_t depthSrvIndex_ = 0;

	// シングルトンインスタンス
	static std::unique_ptr <PostEffect> instance;

	// ブルームのパス数（1/2, 1/4, 1/8 の3段階）
	static const int kBloomPassCount = 3;
	// 3段階分のバッファ配列
	BloomBuffer bloomBuffers_[kBloomPassCount];

	// レンズフレア
	uint32_t lensFlareSrvIndex_ = 0;
	// モーションブラー
	uint32_t velocitySrvIndex_ = 0; // ベロシティバッファ(t6)用
	uint32_t objectMaskSrvIndex_ = 0; // オブジェクトマスク(t7)用
	// ダメージエフェクト
	bool isDamegeFade = true; // フェードアウト
	float damageEffectRatio_ = 0.0f; // 0.0fでエフェクト無し、1.0fで最大ダメージ表現

	// ポインター
	DirectXCommon* dxCommon_ = nullptr;
	WindowAPI* windowAPI_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// レンダーターゲットの生成
	RenderTarget CreateRenderTarget(
		ID3D12Device* device,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		const float clearColor[4],
		ID3D12DescriptorHeap* rtvHeap,
		UINT rtvIndex,
		ID3D12DescriptorHeap* srvHeap,
		UINT srvIndex
	);

	// バックバッファを指定の状態に遷移
	void TransitionBackBuffer(D3D12_RESOURCE_STATES newState);

	// リソースの状態を切り替え
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	// ルートシグネイチャ生成
	void CreateRootSignature();
	// グラフィックスパイプライン生成
	void CreateGraphicsPipeline();

	// ビューポート初期化
	void InitializeViewport();
	// シザリング矩形初期化
	void InitializeScissorRect();
};

