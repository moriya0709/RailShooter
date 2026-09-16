// AnimatorComponent.cpp
#include "AnimatorComponent.h"
#include "GameObject.h"
#include "ModelRendererComponent.h"
#include "TransformComponent.h"
#include "Model.h"

void AnimatorComponent::Initialize() {}

void AnimatorComponent::Update() {
	Model* model = GetModel();
	if (model) {
		// アニメーションの時間を進める更新
		model->Update();
	}
}

void AnimatorComponent::PlayAnimation(const std::string& animationName, float blendTime) {
	Model* model = GetModel();
	if (model) {
		// Model 側で現在アニメーションから指定クリップへクロスフェードする。
		model->PlayAnimation(animationName, blendTime);
	}
}

void AnimatorComponent::StopAnimation() {
	Model* model = GetModel();
	if (model) {
		// クリップを外してポーズ更新を止める。Model 自体の描画は継続する。
		model->SetCurrentAnimation(nullptr);
	}
}

Vector3 AnimatorComponent::GetJointPosition(const std::string& jointName) const {
	Model* model = GetModel();
	if (!model) return { 0.0f, 0.0f, 0.0f };

	// TransformComponent から現在のワールド行列を取得
	Matrix4x4 worldMatrix = MakeIdentity4x4();
	auto transformComp = GetGameObject()->GetComponent<TransformComponent>();
	if (transformComp) {
		worldMatrix = transformComp->GetWorldMatrix();
	}

	return model->GetJointWorldPosition(jointName, worldMatrix);
}

void AnimatorComponent::BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Model* model = GetModel();
	if (model) {
		model->BoneLineUpdate(line, scale, rotate, translate);
	}
}

Model* AnimatorComponent::GetModel() const {
	if (!GetGameObject()) return nullptr;

	// Animator は ModelRendererComponent にのみ依存し、モデルを所有しない。
	auto renderer = GetGameObject()->GetComponent<ModelRendererComponent>();
	if (renderer) {
		return renderer->GetModel();
	}
	return nullptr;
}
