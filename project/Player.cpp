#include "Player.h"
#include "Input.h"
#include "PlayerBulletNormal.h"
#include "PostEffect.h"

void Player::Initialize() {
	reticle = std::make_unique<Sprite>();
	reticle->Initialize("Resource/reticle/reticle.png");
	reticle->SetPosition(reticlePosition);
	reticle->SetRotation(reticleRotation);
	reticle->SetSize(reticleSize);
}

void Player::Update(float deltaTime) {
	switch (currentState) {
	case Player::Normal:
	UpdateNormal(deltaTime);
	break;
	case Player::Death:

	break;

	}



}

void Player::Draw() {
	//reticle->Draw();
}

void Player::OnCollision() {
	hitTimer_ = 0.2f; // 衝突時に0.2秒間（60FPSで約12フレーム）タイマーをセット

	//PostEffect::GetInstance()->SetDamageEffectRatio(1.0f);
}

OBB Player::GetOBB() {
	OBB obb;
	// 1. 中心座標
	obb.center = translate_;

	// 2. ハーフサイズ（TransformのScaleを適用）
	obb.halfExtents = {
		scale_.x * 0.5f,
		scale_.y * 0.5f,
		scale_.z * 0.5f
	};

	// 3. 回転行列から正規化された3軸を取得 (Rotateから回転行列を作成)
	Matrix4x4 rotMat = MakeRotateMatrix(rotate_); // 自身のライブラリの回転行列作成関数[cite: 3]

	obb.axes[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] }; // X軸
	obb.axes[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }; // Y軸
	obb.axes[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] }; // Z軸

	return obb;
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

	// 毎フレームクールダウンを減らす
	if (bulletCoolTime > 0.0f) {
		bulletCoolTime -= 1.0f / 60.0f; // 60 FPSを想定
	}

	// 弾の生成
	if (input->IsMouseButtonPressed(0) && bulletCoolTime <= 0) {
		auto bullet = std::make_unique<PlayerBulletNormal>();
		bullet->Initialize(translate_);
		bullets_.push_back(std::move(bullet));

		bulletCoolTime = 0.5f; // クールタイムをリセット
	}
}

void Player::UpdateNormal(float deltaTime) {
	// タイマーの減算処理と isHit フラグの設定
	if (hitTimer_ > 0.0f) {
		hitTimer_ -= deltaTime;
		isHit = true;
	} else {
		isHit = false;
	}

	Move();

	// ▼▼▼ 追加: 全ての弾を更新 ▼▼▼
	for (auto& bullet : bullets_) {
		bullet->Update();
	}

	// ▼▼▼ 追加: デスフラグが立っている弾をリストから一括削除 ▼▼▼
	bullets_.remove_if([](const std::unique_ptr<PlayerBullet>& bullet) {
		return bullet->IsDead();
		});

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

	// レティクルの更新
	reticleRotation = rotate_.z; // レティクルの回転を機体のロールに合わせる
	reticle->SetPosition(reticlePosition);
	reticle->SetRotation(reticleRotation);
	reticle->SetSize(reticleSize);
	reticle->Update();
}
