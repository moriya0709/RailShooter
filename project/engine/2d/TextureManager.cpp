#include "TextureManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

std::unique_ptr <TextureManager> TextureManager::instance = nullptr;
// ImGuiで0番を使用するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// SRVの数と同数
	textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

// シングルトンインスタンスの取得
TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <TextureManager>();
	}
	return instance.get();
}

void TextureManager::LoadTexture(const std::string& filePath) {
	// 読み込み済みテクスチャを検索
	if (textureDatas.contains(filePath)) {
		OutputDebugStringA(("LoadTexture SKIP: [" + filePath + "]\n").c_str());
		return;
	}
	// テクスチャ枚数上限チェック
	assert(srvManager_->CanAllocate());
	OutputDebugStringA(("LoadTexture NEW: [" + filePath + "]\n").c_str());

	// ファイル読み込み
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr;

	if (filePathW.ends_with(L".dds")) {
		// DDSの読み込み
		hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE,nullptr,image);
	} else {
		// WICの読み込み
		hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 4, mipImages);
	}

	// テクスチャデータを追加して書き込む
	TextureData& textureData = textureDatas[filePath];
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

	// SRV確保
	textureData.srvIndex = srvManager_->Allocate(1);
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	// SRVの生成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	if (textureData.metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	}

	// 生成
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

	// 転送用に生成した中間リソースをテクスチャデータ構造体に格納
	textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);

}

// SRVインデックスの開始番号
uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
	auto it = textureDatas.find(filePath);
	assert(it != textureDatas.end()); // LoadTexture済み前提
	return it->second.srvIndex;
}

// テクスチャ番号からGPUハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {
	auto it = textureDatas.find(filePath);
	assert(it != textureDatas.end());
	return it->second.srvHandleGPU;
}

// メタデータ取得
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
	auto it = textureDatas.find(filePath);
	assert(it != textureDatas.end());
	return it->second.metadata;
}

