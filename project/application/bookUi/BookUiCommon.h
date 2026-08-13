#pragma once
#include "DirectXCommon.h"

class WindowAPI;

class BookUiCommon {
public:
	// 初期化
	void Initialize(WindowAPI* windowAPI);

	// 共通描画設定
	void SetCommonPipelineState();

	// getter
	WindowAPI* GetWindowAPI() const { return windowAPI_; }

	// シングルトンインスタンスの取得
	static BookUiCommon* GetInstance();

private:

	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> VertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> PixelShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> HullShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> DomainShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> outlinePipelineState = nullptr; // アウトライン用

	// シングルトンインスタンス
	static std::unique_ptr <BookUiCommon> instance;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// WindowAPIのポインタ
	WindowAPI* windowAPI_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

};

