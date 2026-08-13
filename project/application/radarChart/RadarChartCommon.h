#pragma once
#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>

#include "DirectXCommon.h"

class RadarChartCommon {
public:
	// 初期化
	void Initialize();

	// 共通描画設定
	void SetCommonPipelineState();

	// シングルトンインスタンスの取得
	static RadarChartCommon* GetInstance();

	RadarChartCommon() = default;
	~RadarChartCommon() = default;
	RadarChartCommon(RadarChartCommon&) = delete;
	RadarChartCommon& operator=(RadarChartCommon&) = delete;

private:
	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;

	// シングルトンインスタンス
	static std::unique_ptr <RadarChartCommon> instance;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
};

