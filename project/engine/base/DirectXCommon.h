#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <debugapi.h>
#include <format>
#include <chrono>
#include <thread>
#include <cassert>

#include "WindowAPI.h"
#include "Logger.h"
#include "StringUtility.h"

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

using namespace Logger;
using namespace StringUtility;

class DirectXCommon {
public:
	HANDLE fenceEvent;
	// デスクリプタステンシル
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// デスクリプタヒープ
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> srvDescriptorHeap; // SRV
	
	// スワップチェイン
	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain = nullptr;
	// スワップチェーンリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	// SwapChain バッファ数に応じて
	D3D12_RESOURCE_STATES backBufferStates[2];

	// 最大SRV数（最大テクスチャ枚数）
	static const uint32_t kMaxSRVCount;

	void Initialize(WindowAPI* windowAPI); // 初期化

	void CreateDevice(); // デバイス関連
	void CreateCommandList(); // コマンドリスト関連
	void CreateSwapChain(); // スワップチェイン関連
	void CreateDepth(); // 深度バッファ関連
	void CreateDescriptor(); // デスクリプタヒープ関連
	void CreateDxcCompiler(); // DXCコンパイラの生成

	void InitializeRTV(); // レンダーターゲットビューの初期化
	void InitializeDSV(); // 深度ステンシルビューの初期化
	void InitializePostEffectDepthSRV(uint32_t index); // ポストエフェクト用の深度SRV初期化
	void InitializeFence(); // フェンスの初期化
	void InitializeViewport(); // ビューポート矩形の初期化
	void InitializeScissorRect(); // シザリング矩形の初期化

	// 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

	// シングルトンインスタンスの取得
	static DirectXCommon* GetInstance();

	// 終了
	void Finalize();

	// getter
	ID3D12Device* GetDevice() const { return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
	size_t GetSwapChainResourceNum() const { return swapChainDesc.BufferCount; }
	ID3D12DescriptorHeap* GetSrvHeap() const { return srvDescriptorHeap.Get(); }
	ID3D12DescriptorHeap* GetRtvHeap() const { return rtvDescriptorHeap.Get(); }
	ID3D12DescriptorHeap* GetDsvHeap() const { return dsvDescriptorHeap.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTVHandle();
	ID3D12Resource* GetDepthStencilResource() const { return depthStencilResource.Get(); }
	uint32_t GetSrvDescriptorSize() const { return descriptorSizeSRV; }

	// デスクリプタヒープ生成
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	// 深度バッファ用リソース生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreatDepthStenCilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height);
	// SRVの指定番号のCPUデスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	// SRVの指定番号のGPUデスクリプタハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	// シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile);
	// バッファリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	// テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	// テクスチャデータの転送
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);
	// テクスチャファイルの読み込み
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	// 指定番号のCPUデスクリプタハンドルを取得
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	// 指定番号のGPUデスクリプタハンドルを取得
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);


private:
	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	// DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	// コマンド
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> commandList = nullptr;

	// depthStencilリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

	// ディスクリプタハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	// ディスクリプタサイズ
	uint32_t descriptorSizeSRV;
	// デスクリプタヒープ
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> rtvDescriptorHeap; // RTV
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap; // DSV

	// デスクリプタヒープ
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{}; // RTV設定


	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{}; // スワップチェイン設定
	// フェンス
	Microsoft::WRL::ComPtr <ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;

	// ビューポート
	D3D12_VIEWPORT viewport{};
	// シザー矩形
	D3D12_RECT scissorRect{};

	// DXCコンパイラ
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;

	// バリア
	D3D12_RESOURCE_BARRIER barrier{};

	// シングルトンインスタンス
	static std::unique_ptr <DirectXCommon> instance;


	// WindowAPI
	WindowAPI* windowAPI_ = nullptr;

	// FPS固定初期化
	void InitializeFixFPS();
	// FPS固定更新
	void UpdateFixFPS();
	// 記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

};

