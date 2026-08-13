#pragma once
#include <windows.h>
#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>
#include <thread>

class DirectXCommon;

class LineCommon {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);
	// 共通描画設定
	void SetCommonPipelineState();

	// シングルトンインスタンスの取得
	static LineCommon* GetInstance();

private:
	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> outlinePipelineState = nullptr; // アウトライン用

	// シングルトンインスタンス
	static std::unique_ptr <LineCommon> instance;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
};

