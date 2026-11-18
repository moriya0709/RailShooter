#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <dxcapi.h>

#include "Calc.h"

class SrvManager;

class ShadowMap {
public:
    // シャドウマップの SRV は、ゲーム全体で共有する shader-visible ヒープから確保する。
    void Initialize(SrvManager* srvManager);

    // ライトの位置と注視点から行列を計算
    void Update();

    // シャドウマップ描画前処理（DSVとしてセット）
    void PreDrawShadow();
    // 深度だけを書き込むシャドウパスの PSO / ルートシグネチャを設定
    void SetPipelineState();
    // シャドウマップ描画後処理（テクスチャとして読み込めるように状態変更）
    void PostDrawShadow();

    // ゲッター
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return dsvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return srvHandleGPU_; }
    Matrix4x4 GetLightVP() const { return lightViewProj_; }

    // シングルトンインスタンスの取得
    static ShadowMap* GetInstance();

private:
    static constexpr uint32_t kShadowMapDsvIndex = 2;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;
    SrvManager* srvManager_ = nullptr;
    uint32_t srvIndex_ = 0;

    uint32_t width_ = 1920;
    uint32_t height_ = 1080;

    Matrix4x4 lightViewProj_;

    // ルートシグネイチャ
    Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
    D3D12_BLEND_DESC blendDesc{};
    D3D12_RASTERIZER_DESC rasterizerDesc{};

    // グラフィックスパイプライン
    Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;	// 通常
    Microsoft::WRL::ComPtr <ID3D12PipelineState> transparentGraphicsPipelineState = nullptr;
    Microsoft::WRL::ComPtr <ID3D12PipelineState> postEffectMaskPipelineState = nullptr;

    // シングルトンインスタンス
    static std::unique_ptr <ShadowMap> instance;

    // ルートシグネイチャの作成
    void CreateRootSignature();
    // グラフィックスパイプラインの生成
    void CreateGraphicsPipeline();
};

