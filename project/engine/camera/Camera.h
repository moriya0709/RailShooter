#pragma once
#include <dinput.h>

#include "Calc.h"

class Camera {
public:
	// デフォルトコンストラクタ
	Camera();
	// 更新
	void Update();

	// デバック用のカメラ操作
	void DebugCameraUpdate();

	// setter
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetViewMatrix(const Matrix4x4& viewMatrix) { this->viewMatrix = viewMatrix; }

	// getter
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }
	const float GetFovY() const { return fovY_; }

	// シングルトンインスタンスの取得
	static Camera* GetInstance();

private:
	// ビュー行列
	Transform transform;
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;

	// プロジェクション行列
	Matrix4x4 projectionMatrix;
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;

	// 合成行列
	Matrix4x4 viewProjectionMatrix;

	// マウスの前回フレームの座標を保存
	Vector2 preMousePos = { 0.0f, 0.0f };

	// 各種スピード設定（デバック）
	float zoomSpeed = 0.05f;
	float panSpeed = 0.05f;
	float rotateSpeed = 0.005f;

	// シングルトンインスタンス
	static Camera* instance;

};

