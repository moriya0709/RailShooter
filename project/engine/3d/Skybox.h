#pragma once
#include <wrl.h>
#include <D3d12.h>
#include <dxcapi.h>

#include "CommonStructs.h"
#include "DirectXCommon.h"

class Camera;

class Skybox {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, std::string textureFilePath);
	// 更新
	void Update();
	// 描画
	void Draw();

	// シングルトンインスタンスの取得
	static Skybox* GetInstance();

	// getter
	std::string GetTextureFilePath() const { return textureFilePath_; }

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

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// テクスチャパス
	std::string textureFilePath_;
	// テクスチャ番号
	uint32_t textureIndex = 0;

	// シングルトンインスタンス
	static std::unique_ptr <Skybox> instance;

	// カメラ
	Camera* camera_ = nullptr;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
};

