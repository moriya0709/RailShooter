#include "Player.h"
#include "Input.h"

void Player::Update(float deltaTime) {
	switch (currentState) {
	case Player::Normal:
	UpdateNormal(deltaTime);
	break;
	case Player::Death:

	break;

	}



}

void Player::Move() {
	auto* input = Input::GetInstance();

	// 毎フレーム入力をリセットする
	playerInput.axisX = 0.0f;
	playerInput.axisY = 0.0f;

	if (input->PushKey(DIK_W)) {
		playerInput.axisY = 1.0f;

	}
	if (input->PushKey(DIK_S)) {
		playerInput.axisY = -1.0f;
	}
	if (input->PushKey(DIK_A)) {
		playerInput.axisX = -1.0f;
	}
	if (input->PushKey(DIK_D)) {
		playerInput.axisX = 1.0f;
	}
}

void Player::UpdateNormal(float deltaTime) {
	Move();

	// 1. 傾きなどの目標値算出
	float targetRoll = -playerInput.axisX * maxRollAngle;
	float targetPitch = -playerInput.axisY * maxPitchAngle;

	Vector3 targetOffset = {
		-playerInput.axisX * maxGForceOffset.x,
		-playerInput.axisY * maxGForceOffset.y,
		0.0f
	};

	// 2. 補間係数 t の計算
	float rotT = 1.0f - std::exp(-rotationSpeed * deltaTime);
	float posT = 1.0f - std::exp(-positionSpeed * deltaTime);
	float moveT = 1.0f - std::exp(-inertiaSpeed * deltaTime); // 慣性用

	// 3. 傾きとG力の補間
	currentPitch = Lerp(currentPitch, targetPitch, rotT);
	currentRoll = Lerp(currentRoll, targetRoll, rotT);
	currentLocalOffset = Lerp(currentLocalOffset, targetOffset, posT);

	rotate_.x = baseRot.x + currentPitch; // X軸: ピッチ(上下の傾き)
	rotate_.y = baseRot.y - currentRoll;  // Y軸: ヨー(カメラと同じ方向を向く)
	rotate_.z = baseRot.z + currentRoll;  // Z軸: ロール(左右の傾き)

	// --- 4. 自機の移動処理（慣性あり） ---
	// 目標とする速度を算出
	Vector2 targetVelocity = {
		playerInput.axisX * moveSpeed,
		playerInput.axisY * moveSpeed
	};

	// 現在の速度を目標速度に向かって滑らかに変化させる（ここで慣性が生まれる）
	velocity.x = Lerp(velocity.x, targetVelocity.x, moveT);
	velocity.y = Lerp(velocity.y, targetVelocity.y, moveT);

	// 速度をもとに画面上の相対位置を更新
	playerPositionOffset.x += velocity.x * deltaTime;
	playerPositionOffset.y += velocity.y * deltaTime;

	// 移動範囲が画面外に出ないようにクランプ制限をかける
	playerPositionOffset.x = std::clamp(playerPositionOffset.x, -moveLimitX, moveLimitX);
	playerPositionOffset.y = std::clamp(playerPositionOffset.y, -moveLimitY, moveLimitY);

	// ⭕ 修正: ローカルな移動量を、カメラの回転を使ってワールド空間の移動量に変換する
	Matrix4x4 matBaseRot = MakeRotateMatrix(baseRot);
	Vector3 worldOffset = VectorTransform(playerPositionOffset, matBaseRot);

	translate_.x = basePos.x + worldOffset.x;
	translate_.y = basePos.y + worldOffset.y;
	translate_.z = basePos.z + worldOffset.z;

}
