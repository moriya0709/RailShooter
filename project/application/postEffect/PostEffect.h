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
	// [16 bytes] 基本フラグ群
	int32_t isInversion;
	int32_t isGrayscale;
	int32_t isRadialBlur;
	int32_t isDistanceFog;

	// [16 bytes] DOF・ハイトフォグフラグ等
	int32_t isDOF;
	int32_t isHeightFog;
	float intensity;
	float pad0;

	// [16 bytes] ブラー設定
	Vector2 blurCenter; // 8 bytes
	float blurWidth;    // 4 bytes
	int32_t blurSamples;// 4 bytes

	// [16 bytes] 距離フォグ設定1
	Vector3 distanceFogColor; // 12 bytes
	float distanceFogStart;   // 4 bytes

	// [16 bytes] 距離フォグ設定2 & カメラ設定
	float distanceFogEnd; // 4 bytes
	float zNear;          // 4 bytes
	float zFar;           // 4 bytes
	float pad1;           // 4 bytes

	// [16 bytes] ハイトフォグ設定1
	Vector3 heightFogColor; // 12 bytes
	float heightFogTop;     // 4 bytes

	// [16 bytes] ハイトフォグ設定2
	float heightFogBottom;  // 4 bytes
	float heightFogDensity; // 4 bytes
	Vector2 pad2;           // 8 bytes

	// [64 bytes] 行列
	Matrix4x4 matInverseViewProjection;

	// [16 bytes] DOF設定
	float focusDistance;
	float focusRange;
	float bokehRadius;
	float pad3;

	// [16 bytes] ブルーム設定
	float bloomThreshold;
	float bloomIntensity;
	float bloomBlurRadius;
	float pad4;

	// [16 bytes] レンズフレア設定
	int32_t isLensFlare;
	int32_t lensFlareGhostCount;
	float lensFlareGhostDispersal;
	float lensFlareHaloWidth;

	// [16 bytes] ACES・色収差設定
	int32_t isACES;
	float caIntensity;
	Vector2 pad5;           // 8 bytes

	// [16 bytes] モーションブラー設定
	int32_t isMotionBlur;
	int32_t motionBlurSamples; // ★正常な回数が読み込まれ、GPUクラッシュが直ります
	float motionBlurScale;
	float pad6;

	// [16 bytes] 画面エフェクトフラグ群
	int32_t isFullScreenCA;
	float fullScreenCAIntensity;
	int32_t isVignette;
	float vignetteIntensity;

	// [16 bytes] ビネット色 & ガウシアンフラグ
	Vector3 vignetteColor;    // 12 bytes
	int32_t isGaussianFilter; // 4 bytes

	// [16 bytes] ガウシアン設定 & アウトライン設定
	float gaussianSigma;    // 4 bytes
	int32_t isOutline;      // 4 bytes
	float outlineThreshold; // 4 bytes
	float pad7;             // 4 bytes (隙間をピッタリ埋めるパディング)

	// [16 bytes] アウトライン色
	Vector4 outlineColor;   // 16 bytes

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
	// 描画
	void Draw();

	// 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

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
	void SetDOF(bool isDOF) { effectData->isDOF = isDOF; }
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
	// ガウシアンフィルタ
	void SetGaussianFilter(bool isGaussianFilter) { effectData->isGaussianFilter = isGaussianFilter; }
	void SetGaussianSigma(float gaussianSigma) { effectData->gaussianSigma = gaussianSigma; }
	// アウトライン
	void SetOutline(bool isOutline) { effectData->isOutline = isOutline; }
	void SetOutlineThreshold(float outlineThreshold) { effectData->outlineThreshold = outlineThreshold; }
	void SetOutlineColor(const Vector4& color) { effectData->outlineColor = color; }


	// ダメージエフェクト
	void SetDamageEffectRatio(float ratio) { damageEffectRatio_ = ratio; }

	// getter
	float GetLensFlareGhostDispersal() { return effectData->lensFlareGhostDispersal; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(uint32_t index) { return rtvHandles[index]; }

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
	// ビューポート
	D3D12_VIEWPORT viewport_;
	// シザー矩形
	D3D12_RECT scissorRect_;

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

