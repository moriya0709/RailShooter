#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>

#include "Calc.h"
#include "CommonStructs.h"

class DirectXCommon;

class Sprite {
public:
	// 初期化
	void Initialize(std::string textureFilePath);
	// 更新
	void Update();
	// 描画
	void Draw();

	// getter
	const Vector2& GetPosition() const { return position; } // 座標
	const float GetRotation() const { return rotation; } // 回転
	const Vector4& GetColor() const { return materialData->color; }
	const Vector2& GetSize() const { return size; }
	const Vector2& GetAnchorPoint() const { return anchorPoint; }
	const bool IsFlipX() const { return isFlipX_; }
	const bool IsFlipY() const { return isFlipY_; }
	const Vector2& GetTextureLeftTop() const { return textureLeftTop; }
	const Vector2& GetTextureSize() const { return textureSize; }
	// setter
	void SetPosition(const Vector2& position) { this->position = position; } // 座標
	void SetRotation(float rotation) { this->rotation = rotation; } // 回転
	void SetColor(const Vector4& color) { materialData->color = color; }
	void SetSize(const Vector2& size) { this->size = size; }
	void SetAnchorPoint(const Vector2& anchorPoint) { this->anchorPoint = anchorPoint; }
	void SetFlipX(bool isFlipX) { this->isFlipX_ = isFlipX; }
	void SetFlipY(bool isFlipY) { this->isFlipY_ = isFlipY; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { this->textureLeftTop = textureLeftTop; }
	void SetTextureSize(const Vector2& textureSize) { this->textureSize = textureSize; }

	// テクスチャ変更
	void ChangeTexture(const std::string& textureFilePath);

private:
	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	uint32_t* indexData = nullptr;
	Material* materialData = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	// テクスチャ
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	// テクスチャ番号
	uint32_t textureIndex = 0;

	Transform transform = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};
	// 座標
	Vector2 position = { 0.0f,0.0f };
	// 回転
	float rotation = 0.0f;
	// サイズ
	Vector2 size = { 640.0f,360.0f };

	// アンカーポイント
	Vector2 anchorPoint = { 0.5f,0.5f };

	// フリップ
	bool isFlipX_ = false;
	bool isFlipY_ = false;

	// テクスチャ範囲指定
	Vector2 textureLeftTop = { 0.0f,0.0f };		// テクスチャ左上座標
	Vector2 textureSize = { 100.0f,100.0f };	// テクスチャ切り出しサイズ

	// テクスチャファイルパス
	std::string textureFilePath_;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// テクスチャサイズ調整
	void AdjustTextureSize();

};

