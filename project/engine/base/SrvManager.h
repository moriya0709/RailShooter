#pragma once
#include <d3d12.h>
#include <cstdint>
#include <wrl.h>

class DirectXCommon;

class SrvManager {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 確保
	uint32_t Allocate(uint32_t num);

	// デスクリプタハンドル計算
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// SRV生成（テクスチャ用）
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
	// SRV生成（Structured Buffer用）
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	// UAV生成
	void CreateUAVforStructuredBuffer(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	// ヒープセットコマンド
	void PreDraw();
	// SRVセットコマンド
	void SetGraphicsRootDescriptorTable(UINT RootparmeterIndex, uint32_t srvIndex);
	// 確保可能チェック
	bool CanAllocate() const;

	// getter
	ID3D12DescriptorHeap* GetDescriptorHeap() const {return descriptorHeap.Get();}

private:
	// 最大SRV数（最大テクスチャ枚数）
	static const uint32_t kMaxSRVConst;
	// SRV用のデスクリプタサイズ
	uint32_t descriptorSize;
	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	// 次に使用するSRVインデックス
	uint32_t useIndex = 0;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

};

