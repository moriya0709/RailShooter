#include "TextureManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cstring>

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

	assert(SUCCEEDED(hr));
	CreateTextureData(filePath, image);

}

void TextureManager::LoadTextureFromRGBA8(const std::string& key, uint32_t width, uint32_t height,
	const std::vector<uint8_t>& pixels) {
	assert(width > 0 && height > 0);
	assert(pixels.size() == static_cast<size_t>(width) * height * 4);

	DirectX::ScratchImage image{};
	const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
	assert(SUCCEEDED(hr));
	const DirectX::Image* destination = image.GetImage(0, 0, 0);
	assert(destination != nullptr);
	std::memcpy(destination->pixels, pixels.data(), pixels.size());
	CreateTextureData(key, image);
}

void TextureManager::CreateTextureData(const std::string& key, DirectX::ScratchImage& image) {
	DirectX::ScratchImage mipImages{};
	HRESULT hr = S_OK;
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
			DirectX::TEX_FILTER_SRGB, 4, mipImages);
	}
	assert(SUCCEEDED(hr));

	TextureData textureData{};
	const auto existing = textureDatas.find(key);
	if (existing != textureDatas.end()) {
		// 動的テキストを更新してもディスクリプタを増やさない。
		textureData.srvIndex = existing->second.srvIndex;
		textureData.srvHandleCPU = existing->second.srvHandleCPU;
		textureData.srvHandleGPU = existing->second.srvHandleGPU;
	} else {
		assert(srvManager_->CanAllocate());
		textureData.srvIndex = srvManager_->Allocate(1);
		textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
		textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
	}

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (textureData.metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	}
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
	textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);
	textureDatas.insert_or_assign(key, std::move(textureData));
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
