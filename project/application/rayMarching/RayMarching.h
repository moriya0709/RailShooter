#pragma once

#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>
#include <memory>
#include <directxmath.h>

#include "Calc.h"

class DirectXCommon;
class SrvManager;
class Camera;

struct CloudParam {
	Matrix4x4 invViewProj;
	Matrix4x4 prevViewProj;

	Vector3 cameraPos;
	float time;						// 時間

	Vector3 sunDir;					// 太陽の位置
	float cloudCoverage;			// 雲の密度

	float cloudBottom;				// 雲の最低座標
	float cloudTop;					// 雲の最高座標
	int isRialLight;				// リアル調ライティング
	int isAnimeLight;				// アニメ調ライティング

	DirectX::XMFLOAT3 cloudOffset;	// uvアニメーション
	int isMotionBlur;				// モーションブラー

	float cloudOpacity;				// 雲の不透明度
	int isStorm;					// 雷雨
	float thunderFrequency;			// 雷の頻度
	float thunderBrightness;		// 雷の明るさ

	float horizonHeight;			// 地平線の高さ

};


class RayMarching {
public:
	// 初期化
	void Initialize(SrvManager* srvManager);
	// 描画
	void Draw();

	// カメラ更新
	void Update(Camera* camera);
	// コンピュートシェーダーを実行
	void ComputeCloud();

	// パラメーター
	void SetInvViewProj(Matrix4x4 invViewProj) { cloudParam->invViewProj = invViewProj; }
	void SetTime(float time) { cloudParam->time = time; }
	void SetSunDir(Vector3 sunDir) { cloudParam->sunDir = sunDir; }
	void SetCloudCoverage(float cloudCoverage) { cloudParam->cloudCoverage = cloudCoverage; }
	void SetCloudBottom(float cloudBottom) { cloudParam->cloudBottom = cloudBottom; }
	void SetCloudTop(float cloudTop) { cloudParam->cloudTop = cloudTop; }
	void SetRialLight(bool isRialLight) { cloudParam->isRialLight = isRialLight; }
	void SetAnimeLight(bool isAnimeLight) { cloudParam->isAnimeLight = isAnimeLight; }
	void SetMotionBlur(bool isMotionBlur) { cloudParam->isMotionBlur = isMotionBlur; }
	void SetCloudOpacity(float cloudOpacity) { cloudParam->cloudOpacity = cloudOpacity; }
	void SetStorm(bool isStorm) { cloudParam->isStorm = isStorm; }
	void SetThunderFrequency(float thunderFrequency) { cloudParam->thunderFrequency = thunderFrequency; }
	void SetThunderBrightness(float thunderBrightness) { cloudParam->thunderBrightness = thunderBrightness; }
	void SetHorizonHeight(float horizonHeight) { cloudParam->horizonHeight = horizonHeight; }

	// getter
	Vector3 GetSunDir() { return cloudParam->sunDir; }
	D3D12_GPU_VIRTUAL_ADDRESS GetCloudParamGPUVirtualAddress() const { return cloudParamResource->GetGPUVirtualAddress(); }

	// シングルトンインスタンスの取得
	static RayMarching* GetInstance();

private:
	// パラメーター
	Microsoft::WRL::ComPtr<ID3D12Resource> cloudParamResource;
	CloudParam* cloudParam;

	// 3Dテクスチャ
	Microsoft::WRL::ComPtr<ID3D12Resource> cloud3DTexture;

	// シングルトンインスタンス
	static std::unique_ptr <RayMarching> instance;

	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> computeRootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> computePipelineState = nullptr;

	// UAV
	D3D12_CPU_DESCRIPTOR_HANDLE uavHandle;
	//SRV
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;

	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;

	// index
	uint32_t srvIndex_;
	uint32_t uavIndex_;

	// 前フレームのカメラ座標
	DirectX::XMFLOAT3 previousCameraPos = { 0.0f, 0.0f, 0.0f };
	// 前フレームのViewProjection行列を保持する変数
	DirectX::XMMATRIX prevViewProjMat;
	// 雲のUVをずらすための蓄積オフセット
	DirectX::XMFLOAT3 cloudOffset = { 0.0f, 0.0f, 0.0f };
	// 初回実行判定用フラグ
	bool isFirstFrame = true;


	// DirectXCommonポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SRVマネージャーポインタ
	SrvManager* srvManager_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
	// コンピュートルートシグネイチャの生成
	void CreateComputeRootSignature();
	// コンピュートパイプラインの生成
	void CreateComputePipeline();

	// 3Dテクスチャリソースの生成
	void Create3DTextureResource();
	// UAVの生成
	void CreateUAVDescriptor();
	// SRVの生成
	void CreateSRVDescriptor();

};