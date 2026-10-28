#pragma once
#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>

#include "DirectXCommon.h"

class SpriteCommon {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void SetCommonPipelineState();
	// enabled が false の場合は、透明でない部分へマスク値 0 を書き込む。
	// 背後のレイマーチング等が書いたマスクを上書きするために必要。
	void SetPostEffectMaskPipelineState(bool enabled = true);

	// シングルトンインスタンスの取得
	static SpriteCommon* GetInstance();

	// ゲッター
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

	SpriteCommon() = default;
	~SpriteCommon() = default;
	SpriteCommon(SpriteCommon&) = delete;
	SpriteCommon& operator=(SpriteCommon&) = delete;

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
	Microsoft::WRL::ComPtr <ID3D12PipelineState> postEffectMaskPipelineState = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> postEffectMaskDisabledPipelineState = nullptr;

	// シングルトンインスタンス
	static std::unique_ptr <SpriteCommon> instance;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
};

