#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <memory>
#include <vector>
#include <algorithm>

#include "TrailEffect.h"
#include "Blend.h"

class TrailEffectManager {
public:
    void Initialize();
    void AddTrail(std::shared_ptr<TrailEffect> trail);
    void UpdateAll();
    void RenderAll();

    // setter
	void SetBlendMode(BlendMode mode) { blendMode = mode; }

    // シングルトンインスタンスの取得
    static TrailEffectManager* GetInstance();

private:
    BlendMode blendMode = kBlendModeAdd;

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

    std::vector<std::shared_ptr<TrailEffect>> m_ActiveTrails;
    // DX12のバッファリソース（全トレイルの頂点データを格納するUploadヒープ）
    Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;

    // 頂点バッファビュー（バッファのGPUアドレスやサイズ、ストライドを保持する構造体）
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView{};

    // カメラ行列(ViewProjection)を送るための定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> m_CameraBuffer;

    // シングルトンインスタンス
    static std::unique_ptr <TrailEffectManager> instance;

    // ルートしグネチャの作成
    void CreateRootSignature();
    // グラフィックスパイプラインの生成
    void CreateGraphicsPipeline();
};