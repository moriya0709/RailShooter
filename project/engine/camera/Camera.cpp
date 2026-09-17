#include "Camera.h"
#include "WindowAPI.h"
#include "Input.h"

Camera* Camera::instance = nullptr;

Camera::Camera()
	: transform({{1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f}})
	, fovY_(0.45f)
	, aspectRatio_(float(WindowAPI::kClientWidth) / float (WindowAPI::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(5000.0f)
	, worldMatrix(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate))
	, viewMatrix(Inverse(worldMatrix))
	, projectionMatrix(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
	, viewProjectionMatrix(Multiply(viewMatrix, projectionMatrix))
{}

void Camera::Update() {
	// ビュー行列
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	viewMatrix = Inverse(worldMatrix);

	// プロジェクション行列
	projectionMatrix = MakePerspectiveFovMatrix(fovY_,aspectRatio_,nearClip_,farClip_);

	// 合成行列
	viewProjectionMatrix = Multiply(viewMatrix,projectionMatrix);

}

void Camera::DebugCameraUpdate() {
	Input* input = Input::GetInstance();

	// 1. マウスの移動量とホイール回転量を取得 (DirectInputは移動量が直接取れる)
	LONG dx = input->GetMouseX();
	LONG dy = input->GetMouseY();
	LONG wheel = input->GetMouseWheel();

	// ---------------------------------------------------------
	// ① マウスホイール：奥行き移動 (Zoom)
	// ---------------------------------------------------------
	if (wheel != 0) {
		// worldMatrix からカメラの「前方向(Z軸)」ベクトルを抽出[cite: 5, 6]
		Vector3 forward = { worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] };

		// ホイールの回転量に合わせて前後に移動
		transform.translate.x += forward.x * (wheel * zoomSpeed);
		transform.translate.y += forward.y * (wheel * zoomSpeed);
		transform.translate.z += forward.z * (wheel * zoomSpeed);
	}

	// ---------------------------------------------------------
	// ② 中ボタン (ボタンインデックス: 2) が押されている時の操作
	// ---------------------------------------------------------
	if (input->IsMouseButtonPressed(2)) {

		if (input->PushKey(DIK_LSHIFT)) {
			// 【Shift + 中ボタン】平行移動 (Pan)
			// worldMatrix からカメラの「右(X軸)」と「上(Y軸)」ベクトルを抽出[cite: 5, 6]
			Vector3 right = { worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2] };
			Vector3 up = { worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2] };

			// マウスを動かした方向と逆にカメラを動かす（空間を掴んで引っ張る挙動）
			// ※ DirectInputではマウスを下へ動かすと dy がプラスになるため、Y軸は足し算で補正
			transform.translate.x += (-right.x * dx + up.x * dy) * panSpeed;
			transform.translate.y += (-right.y * dx + up.y * dy) * panSpeed;
			transform.translate.z += (-right.z * dx + up.z * dy) * panSpeed;

		} else {
			// 【中ボタンのみ】回転操作 (Rotate)
			// マウスのX移動でYaw(Y軸回転)、Y移動でPitch(X軸回転)[cite: 6]
			transform.rotate.y += dx * rotateSpeed;
			transform.rotate.x += dy * rotateSpeed;
		}
	}

	// 変動した Transform をもとに行列を再計算[cite: 5]
	Update();
}

// シングルトンインスタンスの取得
Camera* Camera::GetInstance() {
	if (instance == nullptr) {
		instance = new Camera;
	}
	return instance;
}
