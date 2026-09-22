#pragma once
#include "string"
#include <cstdint>
#include <dxgi1_6.h>
#include <unordered_map>
#include <vector>

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

class DirectXCommon;
class SrvManager;

class TextureManager {
public:
	// テクスチャ1枚分のデータ
	struct TextureData {
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	};

	// テクスチャデータ
	std::unordered_map<std::string,TextureData> textureDatas;

	// 初期化
	void Initialize(DirectXCommon* dxCommon_, SrvManager* srvManager);
	// シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	// テクスチャファイルの読み込み
	// カラーテクスチャは sRGB、法線・粗さ・メタリックなどのデータテクスチャはリニアで読む。
	void LoadTexture(const std::string& filePath, bool isSRGB = true);
	// CPUで生成したRGBA8画像を登録する。既存キーの場合はSRVを再利用する。
	void LoadTextureFromRGBA8(const std::string& key, uint32_t width, uint32_t height,
		const std::vector<uint8_t>& pixels, bool isSRGB = true);
	// SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);
	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);
	// メタデータ取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

private:

	static std::unique_ptr <TextureManager> instance;
	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;

	void CreateTextureData(const std::string& key, DirectX::ScratchImage& image, bool isSRGB);
};
