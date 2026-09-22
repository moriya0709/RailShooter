#include "Object.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "ObjectCommon.h"
#include "Line.h"
#include "LightManager.h"

void Object::Initialize(Camera* camera) {
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

	// アウトライン
	outlineResource = dxCommon_->CreateBufferResource(sizeof(Outline));
	outlineResource->Map(0, nullptr, reinterpret_cast<void**>(&outlineData));
	outlineData->thickness = 0.01f;
	outlineData->color = {1,0,0,0};

	// カメラ
	viewResource = dxCommon_->CreateBufferResource(sizeof(ViewData));
	viewResource->Map(0, nullptr, reinterpret_cast<void**>(&viewData));

	// モーションブラー
	motionBlurResource = dxCommon_->CreateBufferResource(sizeof(MotionBlur));
	motionBlurResource->Map(0, nullptr, reinterpret_cast<void**>(&motionBlurData));
	motionBlurData->isMotionBlur = false;

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

void Object::Update() {
	// Transformの更新
	camera_ = CameraManager::GetInstance()->GetActiveCamera();
	viewData->cameraPos = camera_->GetTranslate();

	// 新しいWVPを計算する前に、現在のWVPを「過去のWVP」として退避させる
	transformationMatrixData->prevWVP = currentWVP_;

	// アニメーション
	if(model_)
		model_->Update();

	// 通常通り、現在のワールド行列を計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	transformationMatrixData->World = worldMatrix;

	// 現在のWVP行列を計算
	currentWVP_ = Multiply(worldMatrix, camera_->GetViewProjectionMatrix());
	transformationMatrixData->WVP = currentWVP_;

}

void Object::BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	model_->BoneLineUpdate(line, scale, rotate, translate);
}

void Object::Draw() {
	if (!model_) {
		return;
	}

	if (model_->IsSkinning()) {
		// アニメーション
		ObjectCommon::GetInstance()->SetAnimationPipelineState();
	} else {
		// 3Dオブジェクトの描画準備
		ObjectCommon::GetInstance()->SetCommonPipelineState();
	}

	// SetGraphicsRootSignature でリセットされたルートパラメータをここで再設定します
	LightManager::GetInstance()->Bind(
		dxCommon_->GetCommandList(), 4, 5, 6, 7
	);

	// wvp用とWorld用のCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	// アウトライン
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(3, outlineResource->GetGPUVirtualAddress());
	// カメラ(ビュー)情報
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(8, viewResource->GetGPUVirtualAddress());
	// モーションブラー
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(9, motionBlurResource->GetGPUVirtualAddress());

	if (model_->IsSkinning()) {
		dxCommon_->GetCommandList()->SetGraphicsRootShaderResourceView(
			15, // スキニング用ルートシグネチャでの MatrixPalette のプロパティ番号
			model_->GetSkinCluster().paletteResource->GetGPUVirtualAddress()
		);
	}

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}

}

void Object::PlayAnimation(const std::string& animationName, float blendTime) {
	model_->PlayAnimation(animationName, blendTime);
}

void Object::StopAnimation() {
	model_->SetCurrentAnimation(nullptr);
}

void Object::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}

Vector3 Object::GetJointPosition(const std::string& jointName) const {
	if (!model_) return { 0.0f, 0.0f, 0.0f };

	// 現在のワールド行列を再計算（または保持しているものを利用）
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	return model_->GetJointWorldPosition(jointName, worldMatrix);
}
