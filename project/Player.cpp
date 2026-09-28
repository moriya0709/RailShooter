#include "Player.h"
#include "Input.h"
#include "PlayerBulletNormal.h"
#include "PlayerBulletMissile.h"
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

void Player::Update() {
	Update(deltaTime_);
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

void Player::UpdateReticle() {
	// レティクルの回転をプレイヤーの回転に応じて変化させる
	reticleRotation[0] = rotate_.z * reticleRotationMultiplier[0];
	reticleRotation[1] = rotate_.z * reticleRotationMultiplier[1];
	reticleRotation[2] = rotate_.z * reticleRotationMultiplier[2];

	Vector2 targetReticlePos = { 960.0f, 540.0f };
	Vector2 targetReticleSize = { 700.0f, 700.0f };

	if (lockedTarget && !lockedTarget->IsDead()) {
		Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
		const Matrix4x4 viewProjMat = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
		const Vector3 targetPos = lockedTarget->GetTranslate();
		const float w = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];

		if (w > 0.0f) {
			const float ndcX = (targetPos.x * viewProjMat.m[0][0] + targetPos.y * viewProjMat.m[1][0] + targetPos.z * viewProjMat.m[2][0] + viewProjMat.m[3][0]) / w;
			const float ndcY = (targetPos.x * viewProjMat.m[0][1] + targetPos.y * viewProjMat.m[1][1] + targetPos.z * viewProjMat.m[2][1] + viewProjMat.m[3][1]) / w;

			// Sprite は 1920x1080 の仮想スクリーン座標で描画し、Game ビューにも同じ比率で出力される。
			targetReticlePos = {
				(ndcX + 1.0f) * 0.5f * 1920.0f,
				(1.0f - ndcY) * 0.5f * 1080.0f
			};
			targetReticleSize = { 150.0f, 150.0f };
		}
	}

	const float reticleSpeed = 15.0f;
	const float reticleT = 1.0f - std::exp(-reticleSpeed * deltaTime_);
	reticlePosition[2].x = Lerp(reticlePosition[2].x, targetReticlePos.x, reticleT);
	reticlePosition[2].y = Lerp(reticlePosition[2].y, targetReticlePos.y, reticleT);
	reticleSize[2].x = Lerp(reticleSize[2].x, targetReticleSize.x, reticleT);
	reticleSize[2].y = Lerp(reticleSize[2].y, targetReticleSize.y, reticleT);

	for (int i = 0; i < 3; ++i) {
		reticle[i]->SetColor(lockOnTimer >= requiredLockOnTime && lockedTarget != nullptr
			? Vector4{ 1.0f, 0.0f, 0.0f, 1.0f }
			: Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		reticle[i]->SetPosition(reticlePosition[i]);
		reticle[i]->SetRotation(reticleRotation[i]);
		reticle[i]->SetSize(reticleSize[i]);
		reticle[i]->Update();
	}
}

void Player::UpdateLockOn(const std::vector<Enemy*>& enemies) {
	// カメラ情報の取得
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 viewMat = camera->GetViewMatrix();
	Matrix4x4 projMat = camera->GetProjectionMatrix();
	Matrix4x4 viewProjMat = Multiply(viewMat, projMat);

	// ▼▼▼ 追加: ミサイル発射可能状態（ロックオン完了状態）の判定 ▼▼▼
	bool isLockOnCompleted = (lockOnTimer >= requiredLockOnTime) && (lockedTarget != nullptr);

	if (isLockOnCompleted) {
		// ロックオン完了状態の場合：現在のターゲットが生存＆カメラ前方にいるかをチェック
		bool keepTarget = false;
		if (!lockedTarget->IsDead()) {
			Vector3 targetPos = lockedTarget->GetTranslate();
			float w = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];

			// カメラの前方にいるなら範囲外に出てもロックオンを継続する
			if (w > 0.0f) {
				keepTarget = true;
			}
		}

		// ターゲットが維持できる状態なら新規検索を行わずにリターン
		if (keepTarget) {
			return;
		}

		// 死亡した・背後に回ったなどで維持できない場合はターゲット解除
		lockedTarget = nullptr;
	}
	

	// --- 既存のターゲット検索ロジック（通常時） ---
	lockedTarget = nullptr;
	float closestDist = lockOnRange;

	// 画面中央の座標と、ロックオンを許可する範囲（ピクセル）
	float centerX = 1920.0f * 0.5f;
	float centerY = 1080.0f * 0.5f;

	for (Enemy* enemy : enemies) {
		if (!enemy || enemy->IsDead()) continue;

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
					lockedTarget = enemy;
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
	// ★ 連射用ターゲットが削除対象なら解除
	if (burstTarget == enemy) {
		burstTarget = nullptr;
	}

	// 発射済みのすべての弾にも通知する
	for (auto* bullet : bullets_) {
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
		auto bulletRightObject = std::make_unique<GameObject>("PlayerBullet");
		auto* bulletRight = bulletRightObject->AddComponent<PlayerBulletNormal>();
		bulletRight->Initialize(rightPos, lockedTarget, launchVelocity_);
		bulletRightObject->Initialize();
		bullets_.push_back(bulletRight);
		bulletObjects_.push_back(std::move(bulletRightObject));

		// 2発目：左の弾を生成
		auto bulletLeftObject = std::make_unique<GameObject>("PlayerBullet");
		auto* bulletLeft = bulletLeftObject->AddComponent<PlayerBulletNormal>();
		bulletLeft->Initialize(leftPos, lockedTarget, launchVelocity_);
		bulletLeftObject->Initialize();
		bullets_.push_back(bulletLeft);
		bulletObjects_.push_back(std::move(bulletLeftObject));


		bulletCoolTime = 0.2f; // クールタイムをリセット
	} else if (input->IsMouseButtonPressed(1)) {
		// ロックオン完了時に6連射の予約をセット
		if (lockOnTimer >= requiredLockOnTime && lockedTarget != nullptr) {
			remainingMissiles = 6;      // 計6発発射
			missileBurstTimer = 0.0f;   // 1発目を即時発射
			isNextRight = true;          // 右側からスタート
			burstTarget = lockedTarget; // 発射中のターゲットを記録

			// ロックオンタイマーをリセット
			lockOnTimer = 0.0f;
		}

		bulletCoolTime = 0.2f; // クールタイムをリセット
	}
}

void Player::UpdateNormal(float deltaTime) {
	const Vector3 previousTranslate = translate_;
	// タイマーの減算処理と isHit フラグの設定
	if (hitTimer_ > 0.0f) {
		hitTimer_ -= deltaTime;
		isHit = true;
	} else {
		isHit = false;
	}

	// ▼▼▼ 追加: ロックオン時間の計測 ▼▼▼
	if (lockedTarget != nullptr) {
		if (lockedTarget == previousLockedTarget) {
			lockOnTimer += deltaTime; // 同じ敵をロックオンし続けているなら増加
		} else {
			lockOnTimer = 0.0f;       // 違う敵に切り替わったらリセット
		}
	} else {
		lockOnTimer = 0.0f;           // ターゲットがいなければリセット
	}
	previousLockedTarget = lockedTarget;

	Move();

	// ▼▼▼ 左右交互の6連射ミサイル発射処理 ▼▼▼
	if (remainingMissiles > 0) {
		missileBurstTimer -= deltaTime;
		if (missileBurstTimer <= 0.0f) {

			// 自機中央座標(translate_)で初期化
			auto missileObject = std::make_unique<GameObject>("PlayerMissile");
			auto* missile = missileObject->AddComponent<PlayerBulletMissile>();
			missile->Initialize(translate_, burstTarget, launchVelocity_);

			// 左右フラグを渡して初期角度をセット
			missile->SetInitAngle(isNextRight);
			missile->SetSpiralDirection(isNextRight);

			missileObject->Initialize();
			bullets_.push_back(missile);
			bulletObjects_.push_back(std::move(missileObject));

			// 次の弾の設定
			isNextRight = !isNextRight;
			remainingMissiles--;
			missileBurstTimer = missileInterval;
		}
	}

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
	velocity.x = Lerp(velocity.x, targetVelocity.x, moveT);
	velocity.y = Lerp(velocity.y, targetVelocity.y, moveT);

	playerPositionOffset.x += velocity.x * deltaTime;
	playerPositionOffset.y += velocity.y * deltaTime;
	playerPositionOffset.x = std::clamp(playerPositionOffset.x, -moveLimitX, moveLimitX);
	playerPositionOffset.y = std::clamp(playerPositionOffset.y, -moveLimitY, moveLimitY);

	Matrix4x4 matBaseRot = MakeRotateMatrix(baseRot);
	Vector3 worldOffset = VectorTransform(playerPositionOffset, matBaseRot);

	// ★ 自機の最新座標を確定
	translate_.x = basePos.x + worldOffset.x;
	translate_.y = basePos.y + worldOffset.y;
	translate_.z = basePos.z + worldOffset.z;
	// 弾の更新単位（1フレーム）と合わせた移動量として保持する。
	launchVelocity_ = translate_ - previousTranslate;

	// 現在フレームの自機移動量を各弾へ渡してから更新する。
	// これによりレールのカーブや速度変化にも弾が追従し、相対速度が保たれる。
	for (auto& bulletObject : bulletObjects_) {
		if (auto* bullet = bulletObject->GetComponent<PlayerBullet>()) {
			bullet->SetInheritedVelocity(launchVelocity_);
		}
		bulletObject->Update();
	}

	bullets_.remove_if([](const PlayerBullet* bullet) { return bullet->IsDead(); });
	bulletObjects_.remove_if([](const std::unique_ptr<GameObject>& bulletObject) {
		auto* bullet = bulletObject->GetComponent<PlayerBullet>();
		return bullet && bullet->IsDead();
	});
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform.translate = translate_;
		GetGameObject()->GetTransform()->transform.rotate = rotate_;
		GetGameObject()->GetTransform()->transform.scale = scale_;
	}

}
