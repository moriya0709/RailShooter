#include "DebugCamera.h"
#include <cmath>

void DebugCamera::Initialize()
{
	// 累積回転行列
	Matrix4x4 matRotX = MakeRotateXMatrix(0.0f);
	Matrix4x4 matRotY = MakeRotateYMatrix(0.0f);
	Matrix4x4 matRotZ = MakeRotateZMatrix(0.0f);
	// 回転行列の合成
	matRot_ = matRotZ * matRotX * matRotY;

	// ローカル座標
	translation_ = { 0.0f,0.0f,-5.0f };
}

void DebugCamera::Update(HWND hwnd)
{
	POINT mousePosition;
	GetCursorPos(&mousePosition);
	ScreenToClient(hwnd, &mousePosition);

	// 初回のみ基準座標を記録
	static bool firstFrame = true;
	if (firstFrame)
	{
		prevMouseX = mousePosition.x;
		prevMouseY = mousePosition.y;
		firstFrame = false;
	}

	// 右クリック時のみ移動
	if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
	{
		const float moveSpeed = 0.002f; // 感度
		translation_.y += (mousePosition.y - prevMouseY) * moveSpeed;
		translation_.x += (mousePosition.x - prevMouseX) * -moveSpeed;
	}

	// 左クリック時のみ回転
	if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
	{
		const float rotateSpeed = -0.002f; // 感度
		
		// 追加回転分の回転行列を生成
		Matrix4x4 matRotDelta = MakeIdentityMatrix();
		matRotDelta *= MakeRotateXMatrix((mousePosition.y - prevMouseY) * rotateSpeed);
		matRotDelta *= MakeRotateYMatrix((mousePosition.x - prevMouseX) * rotateSpeed);
		
		// 累積の回転行列を合成
		matRot_ = matRotDelta * matRot_;
	}

	// マウス位置更新
	prevMouseX = mousePosition.x;
	prevMouseY = mousePosition.y;

	Vector3 target = { 0.0f, 0.0f, 0.0f }; // モデルの中心座標

	// モデル中心に回転軸を合わせる
	Matrix4x4 matTranslateToOrigin = MakeTranslateMatrix(-target);
	Matrix4x4 matTranslateBack = MakeTranslateMatrix(target);

	Matrix4x4 cameraMatrix_ = matTranslateBack * matRot_ * matTranslateToOrigin;

	// カメラの位置を反映（距離分後退させる）
	Matrix4x4 matCameraTranslate = MakeTranslateMatrix(translation_);
	cameraMatrix_ = matCameraTranslate * cameraMatrix_;

	viewMatrix_ = Inverse(cameraMatrix_);

}

// 平行移動行列
Matrix4x4 DebugCamera::MakeTranslateMatrix(const Vector3& translate)
{
	Matrix4x4 result = {};

	result.m[0][0] = 1.0f; // Xスケール
	result.m[1][1] = 1.0f; // Yスケール
	result.m[2][2] = 1.0f; // Zスケール
	result.m[3][0] = translate.x; // X平行移動
	result.m[3][1] = translate.y; // Y平行移動
	result.m[3][2] = translate.z; // Z平行移動
	result.m[3][3] = 1.0f;        // 同次座標

	return result;
}

// 拡大縮小行列
Matrix4x4 DebugCamera::MakeScaleMatrix(const Vector3& scale)
{
	Matrix4x4 result = {};

	result.m[0][0] = scale.x; // Xスケール
	result.m[1][1] = scale.y; // Yスケール
	result.m[2][2] = scale.z; // Zスケール
	result.m[3][3] = 1.0f;    // 同次座標

	return result;
}

// x軸回転行列
Matrix4x4 DebugCamera::MakeRotateXMatrix(float radian)
{
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = -std::sin(radian);
	result.m[2][1] = std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[3][3] = 1.0f;
	return result;
}

// y軸回転行列
Matrix4x4 DebugCamera::MakeRotateYMatrix(float radian)
{
	Matrix4x4 result = {};
	result.m[0][0] = std::cos(radian);
	result.m[0][2] = std::sin(radian);
	result.m[1][1] = 1.0f;
	result.m[2][0] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[3][3] = 1.0f;
	return result;
}

// z軸回転行列
Matrix4x4 DebugCamera::MakeRotateZMatrix(float radian)
{
	Matrix4x4 result = {};
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = -std::sin(radian);
	result.m[1][0] = std::sin(radian);
	result.m[1][1] = std::cos(radian);
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}


// 逆行列
Matrix4x4 DebugCamera::Inverse(const Matrix4x4& m)
{
	Matrix4x4 result = {};

	// 上3x3部分（回転・拡大縮小）の逆行列
	float det =
		m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
		m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
		m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
	float invDet = 1.0f / det;

	// 3x3部分の逆行列（クラメルの公式）
	result.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * invDet;
	result.m[0][1] = -(m.m[0][1] * m.m[2][2] - m.m[0][2] * m.m[2][1]) * invDet;
	result.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * invDet;

	result.m[1][0] = -(m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) * invDet;
	result.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * invDet;
	result.m[1][2] = -(m.m[0][0] * m.m[1][2] - m.m[0][2] * m.m[1][0]) * invDet;

	result.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * invDet;
	result.m[2][1] = -(m.m[0][0] * m.m[2][1] - m.m[0][1] * m.m[2][0]) * invDet;
	result.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * invDet;

	// 平行移動部分の逆変換
	result.m[3][0] = -(result.m[0][0] * m.m[3][0] + result.m[1][0] * m.m[3][1] + result.m[2][0] * m.m[3][2]);
	result.m[3][1] = -(result.m[0][1] * m.m[3][0] + result.m[1][1] * m.m[3][1] + result.m[2][1] * m.m[3][2]);
	result.m[3][2] = -(result.m[0][2] * m.m[3][0] + result.m[1][2] * m.m[3][1] + result.m[2][2] * m.m[3][2]);
	result.m[3][3] = 1.0f;

	return result;
}

// 透視投影行列
Matrix4x4 DebugCamera::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
{
	Matrix4x4 result = {};

	float yScale = 1.0f / std::tan(fovY * 0.5f);
	float xScale = yScale / aspectRatio;

	result.m[0][0] = xScale;
	result.m[1][1] = yScale;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
	result.m[3][3] = 0.0f;

	return result;
}

Matrix4x4 DebugCamera::MakeIdentityMatrix()
{
	static const Matrix4x4 result{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

	return result;
}

// 3次元アフィン変換行列
Matrix4x4 DebugCamera::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	Matrix4x4 result = {};

	// 拡大縮小行列を生成する
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	// 回転行列を生成する
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
	Matrix4x4 rotateXYZMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

	// 平行移動行列を生成する
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	// 合計
	result = Multiply(Multiply(scaleMatrix, rotateXYZMatrix), translateMatrix);

	return result;
}

