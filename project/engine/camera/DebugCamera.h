#pragma once
#include <windows.h>
#include "Calc.h"


class DebugCamera
{
public:

	// 初期化
	void Initialize();
	// 更新
	void Update(HWND hwnd);

	// ビュー行列を取得
	Matrix4x4 GetViewMatrix() { return viewMatrix_; };

private:
	// 累積回転行列
	Matrix4x4 matRot_ = {};
	// ローカル座標
	Vector3 translation_ = { 0,0,-5 };
	// ビュー行列
	Matrix4x4 viewMatrix_ = {};
	// 射影行列
	Matrix4x4 projectionMatrix = {};
	//クライアント領域のサイズ
	const int kClientWidth = 1280;
	const int kClientHeight = 720;

	// 前回のマウス座標
	int prevMouseX = 0;
	int prevMouseY = 0;

	// 逆行列
	Matrix4x4 Inverse(const Matrix4x4& m);
	// 透視投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	Matrix4x4 MakeIdentityMatrix();
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	Matrix4x4 MakeRotateXMatrix(float theta);
	Matrix4x4 MakeRotateYMatrix(float theta);
	Matrix4x4 MakeRotateZMatrix(float theta);
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rot, const Vector3& translate);
};

