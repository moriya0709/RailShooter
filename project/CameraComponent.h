#pragma once

#include "Component.h"
#include "Camera.h"

// Bridges the owner's TransformComponent to the render Camera.
class CameraComponent : public Component {
public:
	void Awake() override;
	void Update() override;

	void DebugCameraUpdate();

	void SetFovY(float fovY) { camera_.SetFovY(fovY); }
	void SetAspectRatio(float aspectRatio) { camera_.SetAspectRatio(aspectRatio); }
	void SetNearClip(float nearClip) { camera_.SetNearClip(nearClip); }
	void SetFarClip(float farClip) { camera_.SetFarClip(farClip); }
	void SetRotate(const Vector3& rotate);
	void SetTranslate(const Vector3& translate);

	const Matrix4x4& GetWorldMatrix() const { return camera_.GetWorldMatrix(); }
	const Matrix4x4& GetViewMatrix() const { return camera_.GetViewMatrix(); }
	const Matrix4x4& GetProjectionMatrix() const { return camera_.GetProjectionMatrix(); }
	const Matrix4x4& GetViewProjectionMatrix() const { return camera_.GetViewProjectionMatrix(); }
	const Vector3& GetRotate() const { return camera_.GetRotate(); }
	const Vector3& GetTranslate() const { return camera_.GetTranslate(); }
	float GetFovY() const { return camera_.GetFovY(); }

	Camera* GetCamera() { return &camera_; }
	const Camera* GetCamera() const { return &camera_; }

private:
	void SyncFromTransform();
	void SyncToTransform();

	Camera camera_;
};
