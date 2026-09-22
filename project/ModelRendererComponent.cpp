#include "ModelRendererComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "ModelManager.h"
#include "ObjectCommon.h"
#include "Camera.h"
#include "LightManager.h"

#include <algorithm>

void ModelRendererComponent::Initialize() {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	// デフォルトカメラをセット
	camera_ = CameraManager::GetInstance()->GetActiveCamera();


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
	outlineData->color = { 1,0,0,0 };

	// カメラ
	viewResource = dxCommon_->CreateBufferResource(sizeof(ViewData));
	viewResource->Map(0, nullptr, reinterpret_cast<void**>(&viewData));

	// モーションブラー
	motionBlurResource = dxCommon_->CreateBufferResource(sizeof(MotionBlur));
	motionBlurResource->Map(0, nullptr, reinterpret_cast<void**>(&motionBlurData));
	motionBlurData->isMotionBlur = false;

	// *Transform* //
	cameraTransform = {
		{1.0f,1.0f,1.0f},
		{0.3f,0.0f,0.0f},
		{0.0f,4.0f,-10.0f}
	};
}

void ModelRendererComponent::Update() {
	// Transformコンポーネントからワールド行列を取得して計算
	auto transform = owner_->GetComponent<TransformComponent>();
	if (transform) {
		// Transformの更新
		camera_ = CameraManager::GetInstance()->GetActiveCamera();
		viewData->cameraPos = camera_->GetTranslate();

		// 新しいWVPを計算する前に、現在のWVPを「過去のWVP」として退避させる
		transformationMatrixData->prevWVP = currentWVP_;

		// 通常通り、現在のワールド行列を計算
		Matrix4x4 worldMatrix = transform->GetWorldMatrix();
		transformationMatrixData->World = worldMatrix;

		// 現在のWVP行列を計算
		currentWVP_ = Multiply(worldMatrix, camera_->GetViewProjectionMatrix());
		transformationMatrixData->WVP = currentWVP_;
		UpdateLodModel();
	}
}

void ModelRendererComponent::Draw() {
	if (!model_) {
		return;
	}
	if (maxDrawDistance_ > 0.0f) {
		const auto* transform = owner_->GetComponent<TransformComponent>();
		const Vector3 cameraPosition = camera_->GetTranslate();
		const float deltaX = transform->transform.translate.x - cameraPosition.x;
		const float deltaY = transform->transform.translate.y - cameraPosition.y;
		const float deltaZ = transform->transform.translate.z - cameraPosition.z;
		const float distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
		if (distanceSquared > maxDrawDistance_ * maxDrawDistance_) {
			return;
		}
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
			15,
			model_->GetSkinCluster().paletteResource->GetGPUVirtualAddress()
		);
	}

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}

}

void ModelRendererComponent::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	modelPath_ = filePath;
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	highLodModel_ = nullptr;
	mediumLodModel_ = nullptr;
	lowLodModel_ = nullptr;
	lodHighModelPath_.clear();
	lodMediumModelPath_.clear();
	lodLowModelPath_.clear();
}

void ModelRendererComponent::SetLodModels(const std::string& highModelPath, const std::string& mediumModelPath,
	const std::string& lowModelPath) {
	if (highModelPath.empty()) {
		SetModel(mediumModelPath.empty() ? lowModelPath : mediumModelPath);
		return;
	}

	lodHighModelPath_ = highModelPath;
	lodMediumModelPath_ = mediumModelPath;
	lodLowModelPath_ = lowModelPath;
	modelPath_ = highModelPath;
	highLodModel_ = ModelManager::GetInstance()->FindModel(highModelPath);
	mediumLodModel_ = mediumModelPath.empty() ? nullptr : ModelManager::GetInstance()->FindModel(mediumModelPath);
	lowLodModel_ = lowModelPath.empty() ? nullptr : ModelManager::GetInstance()->FindModel(lowModelPath);
	// Initialize 前は camera_ が未設定なので、少なくとも High（なければ次の品質）を描画可能にしておく。
	model_ = highLodModel_ ? highLodModel_ : (mediumLodModel_ ? mediumLodModel_ : lowLodModel_);
	UpdateLodModel();
}

void ModelRendererComponent::SetLodDistances(float mediumDistance, float lowDistance) {
	lodMediumDistance_ = (std::max)(0.0f, mediumDistance);
	lodLowDistance_ = (std::max)(lodMediumDistance_, lowDistance);
	UpdateLodModel();
}

void ModelRendererComponent::UpdateLodModel() {
	if (lodHighModelPath_.empty() || !camera_ || !owner_) {
		return;
	}

	const auto* transform = owner_->GetComponent<TransformComponent>();
	if (!transform) {
		return;
	}
	const Vector3 cameraPosition = camera_->GetTranslate();
	const float deltaX = transform->transform.translate.x - cameraPosition.x;
	const float deltaY = transform->transform.translate.y - cameraPosition.y;
	const float deltaZ = transform->transform.translate.z - cameraPosition.z;
	const float distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

	Model* selectedModel = highLodModel_;
	if (distanceSquared >= lodLowDistance_ * lodLowDistance_) {
		selectedModel = lowLodModel_ ? lowLodModel_ : (mediumLodModel_ ? mediumLodModel_ : highLodModel_);
	} else if (distanceSquared >= lodMediumDistance_ * lodMediumDistance_) {
		selectedModel = mediumLodModel_ ? mediumLodModel_ : highLodModel_;
	}
	model_ = selectedModel;
}
