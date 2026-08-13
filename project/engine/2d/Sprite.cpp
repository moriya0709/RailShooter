#include "Sprite.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

void Sprite::Initialize(std::string textureFilePath) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	textureFilePath_ = textureFilePath;

	// *頂点データ* //
	
	// リソース
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
	// バッファリソース
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// データを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// １枚目の三角形
	vertexData[0].position = { 0.0f,1.0f,0.0f,1.0f };// 左下
	vertexData[0].texcoord = { 0.0f,1.0f };
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };

	vertexData[1].position = { 0.0f,0.0f,0.0f,1.0f };// 左上
	vertexData[1].texcoord = { 0.0f,0.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };

	vertexData[2].position = { 1.0f,1.0f,0.0f,1.0f };// 右下
	vertexData[2].texcoord = { 1.0f,1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	// 2枚目の三角形
	vertexData[3].position = { 1.0f,0.0f,0.0f,1.0f };// 右上
	vertexData[3].texcoord = { 1.0f,0.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };

	// *インデックス* //
	
	// リソース
	indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * 6);
	// バッファリソース
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	// インデックス
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;

	// *マテリアル* //

	// リソース
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	// 書き込む
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();
	
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


	// 頂点データ更新
	vertexData[0].position = { left,bottom,0.0f,1.0f };// 左下
	vertexData[1].position = { left,top,0.0f,1.0f };// 左上
	vertexData[2].position = { right,bottom,0.0f,1.0f };// 右下
	vertexData[3].position = { right,top,0.0f,1.0f };// 右上
	vertexData[0].texcoord = { tex_left,tex_bottom };
	vertexData[1].texcoord = { tex_left,tex_top };
	vertexData[2].texcoord = { tex_right,tex_bottom };
	vertexData[3].texcoord = { tex_right,tex_top };


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
	dxCommon_->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

}

// テクスチャ変更
void Sprite::ChangeTexture(const std::string& textureFilePath) {
	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	// indexを差し替える
	textureIndex =
		TextureManager::GetInstance()->GetSrvIndex(textureFilePath);
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
