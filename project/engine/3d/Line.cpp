#include "Line.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "CameraManager.h"

void Line::Initialize(Camera* camera) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	// デフォルトカメラをセット
	camera_ = camera;

	// *座標変換行列* //
	transformationMatrixResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	// 書き込む為のアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	// 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

	// カメラ
	viewResource = dxCommon_->CreateBufferResource(sizeof(ViewData));
	viewResource->Map(0, nullptr, reinterpret_cast<void**>(&viewData));

	// ★追加: 頂点バッファの作成 (線なので頂点は2つ)
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * kMaxVertexCount);
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * kMaxVertexCount;
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// *Transform* //
	transform = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};
	cameraTransform = {
		{1.0f,1.0f,1.0f},
		{0.3f,0.0f,0.0f},
		{0.0f,4.0f,-10.0f}
	};


}

void Line::Update() {
	camera_ = CameraManager::GetInstance()->GetActiveCamera();
	viewData->cameraPos = camera_->GetTranslate();

	// 溜め込んだ頂点データを、GPUのバッファ（vertexData）に一気にコピー
	if (!lineVertices_.empty()) {
		std::memcpy(vertexData, lineVertices_.data(), sizeof(VertexData) * lineVertices_.size());
	}

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	transformationMatrixData->World = worldMatrix * camera_->GetViewProjectionMatrix();
	transformationMatrixData->WVP = Multiply(worldMatrix, camera_->GetViewProjectionMatrix());
}

void Line::Draw() {
	// 描画する線がないなら何もしない
	if (lineVertices_.empty()) {
		return;
	}

	// wvp用とWorld用のCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定

	// 描画
	dxCommon_->GetCommandList()->DrawInstanced(static_cast<UINT>(lineVertices_.size()), 1, 0, 0);

}

void Line::AddLine(const Vector3& start, const Vector3& end) {
	// 最大数を超えないようにチェック
	if (lineVertices_.size() + 2 <= kMaxVertexCount) {
		VertexData vStart{}, vEnd{};
		vStart.position = { start.x, start.y, start.z, 1.0f };
		vEnd.position = { end.x, end.y, end.z, 1.0f };

		lineVertices_.push_back(vStart);
		lineVertices_.push_back(vEnd);
	}
}

void Line::Clear() {
	lineVertices_.clear();
}

// ★追加: 始点と終点の更新
void Line::SetPositions(const Vector3& start, const Vector3& end) {
	// 1つ目の頂点（始点）
	vertexData[0].position = { start.x, start.y, start.z, 1.0f };
	// 2つ目の頂点（終点）
	vertexData[1].position = { end.x, end.y, end.z, 1.0f };
}
