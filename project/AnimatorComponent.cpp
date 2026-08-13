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
		model->PlayAnimation(animationName, blendTime);
	}
}

void AnimatorComponent::StopAnimation() {
	Model* model = GetModel();
	if (model) {
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

	auto renderer = GetGameObject()->GetComponent<ModelRendererComponent>();
	if (renderer) {
		return renderer->GetModel();
	}
	return nullptr;
}