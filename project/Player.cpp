#include "Player.h"
#include "Input.h"
#include "PlayerBulletNormal.h"
#include "PostEffect.h"
#include "Enemy.h"
#include "Camera.h"
#include "CameraManager.h"

void Player::Initialize() {
	for (int i = 0; i < 3; ++i) {
		reticle[i] = std::make_unique<Sprite>();
	}
	reticle[0]->Initialize("Resource/reticle/reticle.png");
	reticle[1]->Initialize("Resource/reticle/reticle2.png");
	reticle[2]->Initialize("Resource/reticle/reticle3.png");

	for (int i = 0; i < 3; ++i) {
		reticle[i]->SetPosition(reticlePosition[i]);
		reticle[i]->SetRotation(reticleRotation[i]);
		reticle[i]->SetSize(reticleSize[i]);
	}
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
	for (int i = 0; i < 3; ++i) {
		reticle[i]->Draw();
	}
}

void Player::UpdateLockOn(const std::vector<std::unique_ptr<Enemy>>& enemies){
	lockedTarget = nullptr;
	float closestDist = lockOnRange;

	// カメラ情報の取得
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 viewMat = camera->GetViewMatrix();
	Matrix4x4 projMat = camera->GetProjectionMatrix();
	Matrix4x4 viewProjMat = Multiply(viewMat, projMat); // 自身の合成関数を使用

	// 画面中央の座標と、ロックオンを許可する範囲（ピクセル）
	float centerX = 1920.0f * 0.5f;
	float centerY = 1080.0f * 0.5f;

	for (const auto& enemy : enemies) {
		if (enemy->IsDead()) continue;

		Vector3 targetPos = enemy->GetTranslate();

		// 1. W成分を計算してカメラの前方にいるか判定
		float w = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];
		
		if (w > 0.0f) {
			// 2. NDCからスクリーン座標へ変換
			float ndcX = (targetPos.x * viewProjMat.m[0][0] + targetPos.y * viewProjMat.m[1][0] + targetPos.z * viewProjMat.m[2][0] + viewProjMat.m[3][0]) / w;
			float ndcY = (targetPos.x * viewProjMat.m[0][1] + targetPos.y * viewProjMat.m[1][1] + targetPos.z * viewProjMat.m[2][1] + viewProjMat.m[3][1]) / w;

			float screenX = (ndcX + 1.0f) * 0.5f * 1920.0f;
			float screenY = (1.0f - ndcY) * 0.5f * 1080.0f;

			// 3. 画面中央の指定範囲内にいるかチェック (std::absを使用するために <cmath> が必要です)
			if (std::abs(screenX - centerX) <= lockOnAreaX && 
			    std::abs(screenY - centerY) <= lockOnAreaY) {
				
				// 4. 範囲内の敵の中で、3D距離が最も近いものを選択
				Vector3 diff = targetPos - translate_;
				float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

				if (dist < closestDist) {
					closestDist = dist;
					lockedTarget = enemy.get();
				}
			}
		}
	}
}

void Player::RemoveBulletTarget(const Enemy* enemy) {
	// プレイヤー自身のロックオン対象が削除対象の敵なら解除
	if (lockedTarget == enemy) {
		lockedTarget = nullptr;
	}

	// 発射済みのすべての弾にも通知する
	for (auto& bullet : bullets_) {
		bullet->RemoveTarget(enemy);
	}
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
		// カメラ情報の取得（アクティブなカメラからワールド行列を取得）
		Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
		Matrix4x4 cameraWorld = camera->GetWorldMatrix();

		// カメラの「右方向(X軸)」ベクトルを抽出
		Vector3 right = {
			cameraWorld.m[0][0],
			cameraWorld.m[0][1],
			cameraWorld.m[0][2]
		};

		float offsetRight = 1.5f; // 肩幅（どれくらい左右に離すか）

		// --- 右肩の座標を計算 ---
		Vector3 rightPos;
		rightPos.x = translate_.x + right.x * offsetRight;
		rightPos.y = translate_.y + right.y * offsetRight;
		rightPos.z = translate_.z + right.z * offsetRight;

		// --- 左肩の座標を計算 ---
		Vector3 leftPos;
		leftPos.x = translate_.x - right.x * offsetRight;
		leftPos.y = translate_.y - right.y * offsetRight;
		leftPos.z = translate_.z - right.z * offsetRight;


		// 1発目：右の弾を生成
		auto bulletRight = std::make_unique<PlayerBulletNormal>();
		bulletRight->Initialize(rightPos, lockedTarget);
		bullets_.push_back(std::move(bulletRight));

		// 2発目：左の弾を生成
		auto bulletLeft = std::make_unique<PlayerBulletNormal>();
		bulletLeft->Initialize(leftPos, lockedTarget);
		bullets_.push_back(std::move(bulletLeft));


		bulletCoolTime = 0.2f; // クールタイムをリセット
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

	// レティクルの回転を機体のロールに合わせる
	reticleRotation[0] = rotate_.z * 3.0f;
	reticleRotation[1] = rotate_.z;
	reticleRotation[2] = rotate_.z * 2.0f;

	// --- レティクル3(ロックオンカーソル)の目標値設定 ---
	Vector2 targetReticlePos = { 960.0f, 540.0f };  // デフォルトは画面中央
	Vector2 targetReticleSize = { 700.0f, 700.0f }; // デフォルトのサイズ

	if (lockedTarget && !lockedTarget->IsDead()) {
		// アクティブなカメラを取得
		Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
		Matrix4x4 viewMat = camera->GetViewMatrix();
		Matrix4x4 projMat = camera->GetProjectionMatrix();

		// View行列とProjection行列の乗算 (※自身のライブラリの行列乗算関数を使用)
		Matrix4x4 viewProjMat = Multiply(viewMat, projMat);

		Vector3 targetPos = lockedTarget->GetTranslate();

		// W成分の計算（カメラの前方にいるかどうかの判定に使用）
		float w = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];

		// w が 0より大きい場合のみ（カメラの背後にいる場合は除外）
		if (w > 0.0f) {
			// NDC (正規化デバイス座標系: -1.0 ～ 1.0) への変換
			float ndcX = (targetPos.x * viewProjMat.m[0][0] + targetPos.y * viewProjMat.m[1][0] + targetPos.z * viewProjMat.m[2][0] + viewProjMat.m[3][0]) / w;
			float ndcY = (targetPos.x * viewProjMat.m[0][1] + targetPos.y * viewProjMat.m[1][1] + targetPos.z * viewProjMat.m[2][1] + viewProjMat.m[3][1]) / w;

			// ロックオン時の目標位置と縮小サイズを設定
			targetReticlePos.x = (ndcX + 1.0f) * 0.5f * 1920.0f;
			targetReticlePos.y = (1.0f - ndcY) * 0.5f * 1080.0f; // Y軸は反転させる

			// ★ロックオン時にどれくらい縮めるか（お好みで調整してください）
			targetReticleSize = { 150.0f, 150.0f };
		}
	}

	// --- イージング（滑らかな補間）の計算 ---
	// 追従スピード（値が大きいほど素早く移動・縮小します。10.0f〜20.0fあたりがお勧めです）
	float reticleSpeed = 15.0f;
	float reticleT = 1.0f - std::exp(-reticleSpeed * deltaTime);

	// 現在の位置とサイズを目標値に向かって滑らかに変化させる
	reticlePosition[2].x = Lerp(reticlePosition[2].x, targetReticlePos.x, reticleT);
	reticlePosition[2].y = Lerp(reticlePosition[2].y, targetReticlePos.y, reticleT);

	reticleSize[2].x = Lerp(reticleSize[2].x, targetReticleSize.x, reticleT);
	reticleSize[2].y = Lerp(reticleSize[2].y, targetReticleSize.y, reticleT);

	// レティクルの更新
	for (int i = 0; i < 3; ++i) {
		reticle[i]->SetPosition(reticlePosition[i]);
		reticle[i]->SetRotation(reticleRotation[i]);
		reticle[i]->SetSize(reticleSize[i]);
		reticle[i]->Update();
	}
}
