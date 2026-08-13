#pragma once
#include <windows.h>
#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>
#include <thread>

class Camera;
class DirectXCommon;

class ObjectCommon {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void SetCommonPipelineState(); // 通常
	void SetAnimationPipelineState(); // アニメーション
	void SetOutlinePipelineState(); // アウトライン

	// シングルトンインスタンスの取得
	static ObjectCommon* GetInstance();

	// setter
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }

	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }
	ID3D12RootSignature* GetComputeRootSignature()const { return computeRootSignature.Get(); }
	ID3D12PipelineState* GetComputePipelineState()const {return computePipelineState.Get(); }

	ObjectCommon() = default;
	~ObjectCommon() = default;
	ObjectCommon(ObjectCommon&) = delete;
	ObjectCommon& operator=(ObjectCommon&) = delete;

private:
	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> animationRootSignature = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> computeRootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[6] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;	// 通常
	Microsoft::WRL::ComPtr <ID3D12PipelineState> animationPipelineState = nullptr;	// アニメーション
	Microsoft::WRL::ComPtr <ID3D12PipelineState> outlinePipelineState = nullptr;	// アウトライン用

	// コンピュートパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> computePipelineState = nullptr;

	// シングルトンインスタンス
	static std::unique_ptr <ObjectCommon> instance;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// デフォルトカメラ
	Camera* defaultCamera_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();				// 通常
	void CreateAnimationRootSignature();	// アニメーション
	void CreateComputeRootSignature();		// コンピュート
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();			// 通常
	void CreateGraphicsAnimationPipeline(); // アニメーション
	void CreateGraphicsOutlinePipeline();	// アウトライン用
	// コンピュートパイプラインの生成
	void CreateComputePipeline();

	// 

};

