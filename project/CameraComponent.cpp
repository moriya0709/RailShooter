#include "CameraComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

void CameraComponent::Awake() {
	SyncFromTransform();
	camera_.Update();
}

void CameraComponent::Update() {
	SyncFromTransform();
	camera_.Update();
}

void CameraComponent::DebugCameraUpdate() {
	SyncFromTransform();
	camera_.DebugCameraUpdate();
	SyncToTransform();
}

void CameraComponent::SetRotate(const Vector3& rotate) {
	camera_.SetRotate(rotate);
	if (owner_) {
		owner_->GetTransform()->SetRotate(rotate);
	}
}

void CameraComponent::SetTranslate(const Vector3& translate) {
	camera_.SetTranslate(translate);
	if (owner_) {
		owner_->GetTransform()->SetTranslate(translate);
	}
}

void CameraComponent::SyncFromTransform() {
	if (!owner_) {
		return;
	}

	const TransformComponent* transform = owner_->GetTransform();
	camera_.SetRotate(transform->GetRotate());
	camera_.SetTranslate(transform->GetTranslate());
}

void CameraComponent::SyncToTransform() {
	if (!owner_) {
		return;
	}

	TransformComponent* transform = owner_->GetTransform();
	transform->SetRotate(camera_.GetRotate());
	transform->SetTranslate(camera_.GetTranslate());
}
