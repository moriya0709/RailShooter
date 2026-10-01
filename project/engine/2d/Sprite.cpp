#include "Sprite.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

void Sprite::Initialize(std::string textureFilePath) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	textureFilePath_ = textureFilePath;

	// *頂点データ* //
	
	// リソース
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * kMeshVertexCount);
	// バッファリソース
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * kMeshVertexCount;
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// データを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	for (uint32_t row = 0; row < kMeshRowCount; ++row) {
		for (uint32_t column = 0; column < kMeshColumnCount; ++column) {
			const uint32_t vertexIndex = row * kMeshColumnCount + column;
			vertexData[vertexIndex].position = { static_cast<float>(column) / (kMeshColumnCount - 1),
				static_cast<float>(row) / (kMeshRowCount - 1), 0.0f, 1.0f };
			vertexData[vertexIndex].texcoord = { static_cast<float>(column) / (kMeshColumnCount - 1),
				static_cast<float>(row) / (kMeshRowCount - 1) };
			vertexData[vertexIndex].normal = { 0.0f, 0.0f, -1.0f };
		}
	}

	// *インデックス* //
	
	// リソース
	indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * kMeshIndexCount);
	// バッファリソース
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * kMeshIndexCount;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	// インデックス
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	uint32_t index = 0;
	for (uint32_t row = 0; row < kMeshRowCount - 1; ++row) {
		for (uint32_t column = 0; column < kMeshColumnCount - 1; ++column) {
			const uint32_t topLeft = row * kMeshColumnCount + column;
			const uint32_t topRight = topLeft + 1;
			const uint32_t bottomLeft = topLeft + kMeshColumnCount;
			const uint32_t bottomRight = bottomLeft + 1;
			indexData[index++] = topLeft;
			indexData[index++] = bottomLeft;
			indexData[index++] = topRight;
			indexData[index++] = bottomLeft;
			indexData[index++] = bottomRight;
			indexData[index++] = topRight;
		}
	}

	// *マテリアル* //

	// リソース
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	// 書き込む
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->emissive = { 0.0f, 0.0f, 0.0f };
	
	// *座標変換行列* //

	// リソース
	transformationMatrixResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	// 書き込む
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

	// *テクスチャ* //

	// 読み込み
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	// 番号取得
	textureIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath_);
	// テクスチャサイズ調整
	AdjustTextureSize();
	
}

// 更新
void Sprite::Update() {
	// 座標
	transform.translate = { position.x,position.y,0.0f };
	// 回転
	transform.rotate = { 0.0f,0.0f,rotation };
	// サイズ
	transform.scale = { size.x,size.y,1.0f };

	// アンカーポイント
	float left = 0.0f - anchorPoint.x;
	float right = 1.0f - anchorPoint.x;
	float top = 0.0f - anchorPoint.y;
	float bottom = 1.0f - anchorPoint.y;

	// 左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}
	// 上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

	// テクスチャ範囲指定
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);
	float tex_left = textureLeftTop.x / metadata.width;
	float tex_right = (textureLeftTop.x + textureSize.x) / metadata.width;
	float tex_top = textureLeftTop.y / metadata.height;
	float tex_bottom = (textureLeftTop.y + textureSize.y) / metadata.height;


	const uint32_t meshColumnCount = meshDeformationEnabled_ ? kMeshColumnCount : kDefaultMeshColumnCount;
	const uint32_t meshRowCount = meshDeformationEnabled_ ? kMeshRowCount : kDefaultMeshRowCount;
	// 頂点データ更新
	for (uint32_t row = 0; row < meshRowCount; ++row) {
		const float verticalRatio = static_cast<float>(row) / (meshRowCount - 1);
		for (uint32_t column = 0; column < meshColumnCount; ++column) {
			const float horizontalRatio = static_cast<float>(column) / (meshColumnCount - 1);
			const uint32_t vertexIndex = row * meshColumnCount + column;
			const Vector2& offset = meshOffsets_[vertexIndex];
			vertexData[vertexIndex].position = {
				left + (right - left) * horizontalRatio + offset.x,
				top + (bottom - top) * verticalRatio + offset.y, 0.0f, 1.0f };
			vertexData[vertexIndex].texcoord = {
				tex_left + (tex_right - tex_left) * horizontalRatio,
				tex_top + (tex_bottom - tex_top) * verticalRatio };
		}
	}
	uint32_t index = 0;
	for (uint32_t row = 0; row < meshRowCount - 1; ++row) {
		for (uint32_t column = 0; column < meshColumnCount - 1; ++column) {
			const uint32_t topLeft = row * meshColumnCount + column;
			const uint32_t topRight = topLeft + 1;
			const uint32_t bottomLeft = topLeft + meshColumnCount;
			const uint32_t bottomRight = bottomLeft + 1;
			indexData[index++] = topLeft;
			indexData[index++] = bottomLeft;
			indexData[index++] = topRight;
			indexData[index++] = bottomLeft;
			indexData[index++] = bottomRight;
			indexData[index++] = topRight;
		}
	}
	activeMeshIndexCount_ = index;
	indexBufferView.SizeInBytes = sizeof(uint32_t) * activeMeshIndexCount_;


	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(WindowAPI::kClientWidth), float(WindowAPI::kClientHeight), 0.0f, 100.0f);
	// WVPmatrixを作る
	Matrix4x4 worldViewProjectionMatrix = Multiply(Multiply(worldMatrix, viewMatrix), projectionMatrix);
	transformationMatrixData->WVP = worldViewProjectionMatrix;   // WVP行列を設定
	transformationMatrixData->World = worldMatrix; // World行列を設定
}

void Sprite::Draw() {
	// *設定* //

	// 頂点データ
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);// VBVを設定
	// インデックス
	dxCommon_->GetCommandList()->IASetIndexBuffer(&indexBufferView);
	
	// *場所を設定* //

	// マテリアル
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// 座標変換行列
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	
	// SRVのDescriptorTableの先頭を設定
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
	// インデックスを使って描画
	dxCommon_->GetCommandList()->DrawIndexedInstanced(activeMeshIndexCount_, 1, 0, 0, 0);

}

// テクスチャ変更
void Sprite::ChangeTexture(const std::string& textureFilePath) {
	textureFilePath_ = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	// indexを差し替える
	textureIndex =
		TextureManager::GetInstance()->GetSrvIndex(textureFilePath);
	AdjustTextureSize();
}

// テクスチャサイズ調整
void Sprite::AdjustTextureSize() {
	// テクスチャメタデータ取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	textureSize.x = static_cast<float>(metadata.width);
	textureSize.y = static_cast<float>(metadata.height);
	// 画像サイズをテクスチャサイズに合わせる
	size = textureSize;

}
