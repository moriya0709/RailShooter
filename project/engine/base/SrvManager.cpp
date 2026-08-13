#include "SrvManager.h"
#include "DirectXCommon.h"

const uint32_t SrvManager::kMaxSRVConst = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	// デスクリプタヒープの生成
	descriptorHeap = dxCommon_->GetSrvHeap();

	descriptorSize =
		dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

uint32_t SrvManager::Allocate(uint32_t num) {
	assert(useIndex + num <= kMaxSRVConst);

	int index = useIndex;
	// まとめて確保した分、インデックスを進める
	useIndex += num;

	OutputDebugStringA(("SrvManager::Allocate() -> index=" + std::to_string(index) + ", count=" + std::to_string(num) + "\n").c_str());

	return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(mipLevels);
	
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = numElements;
	instancingSrvDesc.Buffer.StructureByteStride = structureByteStride;

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &instancingSrvDesc, GetCPUDescriptorHandle(srvIndex));

}

void SrvManager::CreateUAVforStructuredBuffer(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = structureByteStride;

	// SrvManager内部の関数を使って確実に正しいCPUハンドルを取得
	// ※ここの dxCommon_->GetDevice() や GetCPUDescriptorHandle(index) は、
	// SrvManagerの CreateSRVforStructuredBuffer と全く同じ書き方にしてください。
	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		pResource,
		nullptr,
		&uavDesc,
		GetCPUDescriptorHandle(index)
	);
}

void SrvManager::PreDraw() {
	// 描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex) {
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

bool SrvManager::CanAllocate() const {
	if (useIndex <= kMaxSRVConst) {
		return true;
	} else {
		return false;
	}
}
