#include "Player.h"
#include "Input.h"
#include "PlayerBulletNormal.h"
#include "PlayerBulletMissile.h"
#include "PostEffect.h"
#include "Enemy.h"
#include "Camera.h"
#include "CameraManager.h"

void Player::Initialize() {
	// レティクル
	reticleObject[0] = GameObject::FindByName("reticle_1");
	reticleObject[1] = GameObject::FindByName("reticle_2");
	reticleObject[2] = GameObject::FindByName("reticle_3");

	for (int index = 0; index < 3; ++index) {
		if (!reticleObject[index]) {
			OutputDebugStringA("Player: reticle object is missing from the level.\n");
			continue;
		}
		reticleSprite[index] = reticleObject[index]->GetComponent<SpriteRendererComponent>();
		reticleTransform[index] = reticleObject[index]->GetComponent<RectTransformComponent>();
	}

	// 矢印の初期化
	arrow = std::make_unique<Sprite>();
	arrow->Initialize("Resource/arrow/arrow.png");
	arrow->SetSize({ 100.0f, 100.0f });
	arrow->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });

	// 初期状態を Normal に設定
	currentState = Normal;
	// 体力
	hp_ = 3;
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
	UpdateDeath(deltaTime);

	break;

	}



}

void Player::ResetRailOffset() {
	playerInput = {};
	currentPitch = 0.0f;
	currentYaw = 0.0f;
	currentRoll = 0.0f;
	angularVelocity = {};
	currentLocalOffset = {};
	playerPositionOffset = {};
	velocity = {};
	launchVelocity_ = {};

	translate_ = basePos;
	rotate_ = baseRot;
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform.translate = translate_;
		GetGameObject()->GetTransform()->transform.rotate = rotate_;
	}
}

void Player::Draw() {
	
	// 矢印の描画
	if (isArrowActive) {
		arrow->Draw();
	}
}

void Player::UpdateReticle() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	const Matrix4x4 viewProjMat = Multiply(
		camera->GetViewMatrix(),
		camera->GetProjectionMatrix()
	);

	// レティクルの回転をプレイヤーの回転に応じて変化させる
	reticleRotation[0].z = rotate_.z * reticleRotationMultiplier[0];
	reticleRotation[1].z = rotate_.z * reticleRotationMultiplier[1];
	reticleRotation[2].z = rotate_.z * reticleRotationMultiplier[2];

	Vector3 targetReticlePos = { 960.0f, 540.0f, 0.0f };
	Vector3 targetReticleSize = { 700.0f, 700.0f, 0.0f };

	if (lockedTarget && !lockedTarget->IsDead()) {
		const Vector3 targetPos = lockedTarget->GetTranslate();
		const float clipX = targetPos.x * viewProjMat.m[0][0] + targetPos.y * viewProjMat.m[1][0] + targetPos.z * viewProjMat.m[2][0] + viewProjMat.m[3][0];
		const float clipY = targetPos.x * viewProjMat.m[0][1] + targetPos.y * viewProjMat.m[1][1] + targetPos.z * viewProjMat.m[2][1] + viewProjMat.m[3][1];
		const float clipW = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];

		if (clipW > 0.0f) {
			const float ndcX = clipX / clipW;
			const float ndcY = clipY / clipW;

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
	reticlePosition.x = Lerp(reticlePosition.x, targetReticlePos.x, reticleT);
	reticlePosition.y = Lerp(reticlePosition.y, targetReticlePos.y, reticleT);
	reticleSize.x = Lerp(reticleSize.x, targetReticleSize.x, reticleT);
	reticleSize.y = Lerp(reticleSize.y, targetReticleSize.y, reticleT);

	constexpr Vector2 kReticleBaseSize[] = { {800.0f, 800.0f}, {800.0f, 800.0f}, {700.0f, 700.0f} };
	for (int i = 0; i < 3; ++i) {
		if (!reticleSprite[i] || !reticleTransform[i]) {
			continue;
		}
		reticleSprite[i]->SetColor(lockOnTimer >= requiredLockOnTime && lockedTarget != nullptr
			? Vector4{ 1.0f, 0.0f, 0.0f, 1.0f }
			: Vector4{ 0.5f, 0.5f, 1.0f, 1.0f });
		reticleTransform[2]->position = { reticlePosition.x, reticlePosition.y };
		reticleTransform[i]->rotation = reticleRotation[i].z;
		reticleTransform[2]->scale = {
			reticleSize.x / kReticleBaseSize[i].x,
			reticleSize.y / kReticleBaseSize[i].y,
		};
		reticleSprite[i]->Update();
	}

	// 矢印はロックオン完了前だけ、レティクル範囲外の最寄りの敵を指す。
	const Vector2 center = { 960.0f, 540.0f };
	const float arrowRadius = 400.0f;

	const bool isLockedOn =
		lockedTarget != nullptr &&
		lockOnTimer >= requiredLockOnTime;

	isArrowActive = false;
	if (!isLockedOn && indicatorTarget && !indicatorTarget->IsDead()) {
		const Vector3 targetPos = indicatorTarget->GetTranslate();
		const float clipX = targetPos.x * viewProjMat.m[0][0] + targetPos.y * viewProjMat.m[1][0] + targetPos.z * viewProjMat.m[2][0] + viewProjMat.m[3][0];
		const float clipY = targetPos.x * viewProjMat.m[0][1] + targetPos.y * viewProjMat.m[1][1] + targetPos.z * viewProjMat.m[2][1] + viewProjMat.m[3][1];
		const float clipW = targetPos.x * viewProjMat.m[0][3] + targetPos.y * viewProjMat.m[1][3] + targetPos.z * viewProjMat.m[2][3] + viewProjMat.m[3][3];

		Vector2 direction = { clipX, -clipY };
		if (clipW < 0.0f) {
			direction.x = -direction.x;
			direction.y = -direction.y;
		}

		bool isOutsideReticle = clipW <= 0.0f;
		if (clipW > 0.0f) {
			const Vector2 screenPos = {
				(clipX / clipW + 1.0f) * targetReticlePos.x,
				(1.0f - clipY / clipW) * targetReticlePos.y,
			};
			isOutsideReticle =
				std::abs(screenPos.x - center.x) > lockOnAreaX ||
				std::abs(screenPos.y - center.y) > lockOnAreaY;
		}

		const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		if (isOutsideReticle && length > 0.001f) {
			direction.x /= length;
			direction.y /= length;

			arrow->SetPosition({
				center.x + direction.x * arrowRadius,
				center.y + direction.y * arrowRadius,
			});
			arrow->SetRotation(std::atan2(direction.y, direction.x));
			arrow->Update();
			isArrowActive = true;
		}
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

	// 矢印のターゲットもリセット
	indicatorTarget = nullptr;
	float closestIndicatorDist = FLT_MAX;

	// レティクルの座標と、ロックオンを許可する範囲（ピクセル）
	float centerX = reticlePosition.x;
	float centerY = reticlePosition.y;

	for (Enemy* enemy : enemies) {
		if (!enemy || enemy->IsDead()) continue;

		// 矢印のターゲット判定
		const Vector3 diff = enemy->GetTranslate() - translate_;
		const float distSq = Dot(diff, diff);

		if (distSq < closestIndicatorDist) {
			closestIndicatorDist = distSq;
			indicatorTarget = enemy;
		}

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

void Player::OnCollision(int damage) {
	hitTimer_ = 0.2f; // 衝突時に0.2秒間（60FPSで約12フレーム）タイマーをセット

	hp_ = (std::max)(0, hp_ - damage);
	if (hp_ == 0) {
		currentState = Death;
		return;
	}

	PostEffect::GetInstance()->SetDamageEffectRatio(0.4f);
	CameraManager::GetInstance()->GetActiveCamera()->StartShake(0.5f, 0.2f);
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
		bulletCoolTime -= deltaTime_;
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


		// ★ 左右フラグに応じて片方だけ発射する
		if (isNextNormalRight) {
			auto bulletRightObject = std::make_unique<GameObject>("PlayerBullet");
			auto* bulletRight = bulletRightObject->AddComponent<PlayerBulletNormal>();
			bulletRight->Initialize(rightPos, lockedTarget, launchVelocity_);
			bulletRightObject->Initialize();
			bullets_.push_back(bulletRight);
			bulletObjects_.push_back(std::move(bulletRightObject));
		} else {
			auto bulletLeftObject = std::make_unique<GameObject>("PlayerBullet");
			auto* bulletLeft = bulletLeftObject->AddComponent<PlayerBulletNormal>();
			bulletLeft->Initialize(leftPos, lockedTarget, launchVelocity_);
			bulletLeftObject->Initialize();
			bullets_.push_back(bulletLeft);
			bulletObjects_.push_back(std::move(bulletLeftObject));
		}

		// 次の弾は逆から撃つようにフラグを反転
		isNextNormalRight = !isNextNormalRight;


		bulletCoolTime = kBulletCoolTime; // クールタイムをリセット
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

		bulletCoolTime = kBulletCoolTime; // クールタイムをリセット
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

	// 移動処理
	if (isMoveEnabled_ && deltaTime > 0.0f) {
		Move();
	}

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

	// 1. 各軸の入力値
	float pitchInput = -playerInput.axisY;
	float yawInput = playerInput.axisX;
	float rollInput = -playerInput.axisX;

	// 現在の角度が「入力している方向の最大値」にどれくらい近いかを計算し、
	// 最大値に近いほど加速の倍率を落とす(1.0 -> 0.0)ための補助関数
	auto CalcLimitRatio = [](float current, float maxAngle, float input) {
		if (input == 0.0f) return 1.0f;
		// 入力がプラス方向ならプラスの上限、マイナス方向ならマイナスの下限に対する到達割合
		float ratio = (input > 0.0f) ? (current / maxAngle) : (current / -maxAngle);
		// 割合を 0.0 ～ 1.0 に収め、限界に近づくほど 0 になるように反転
		return 1.0f - std::clamp(ratio, 0.0f, 1.0f);
		};

	// 限界値までの余裕（0.0 ～ 1.0）を計算
	float pitchRatio = CalcLimitRatio(currentPitch, maxPitchAngle, pitchInput);
	float yawRatio = CalcLimitRatio(currentYaw, maxYawAngle, yawInput);
	float rollRatio = CalcLimitRatio(currentRoll, maxRollAngle, rollInput);

	// 入力に応じて角速度を加算（限界に近づくほど ratio が 0 になり加速しなくなる）
	angularVelocity.x += pitchInput * pitchRatio * rotAccel * deltaTime;
	angularVelocity.y += yawInput * yawRatio * rotAccel * deltaTime;
	angularVelocity.z += rollInput * rollRatio * rotAccel * deltaTime;

	// ▼▼▼ 修正：摩擦を動的に変更して慣性を消す ▼▼▼
	// 基本の摩擦
	float currentFrictionX = rotFriction;
	float currentFrictionYZ = rotFriction;

	// 縦入力がない、または現在の回転方向と逆の入力（切り返し）をしている場合は摩擦を強くして慣性を消す
	if (pitchInput == 0.0f || (angularVelocity.x * pitchInput < 0.0f)) {
		currentFrictionX = rotFriction * 5.0f; // 摩擦を5倍にして即座に止める
	}
	// 横入力がない、または切り返しの場合は摩擦を強くする
	if (yawInput == 0.0f || (angularVelocity.y * yawInput < 0.0f)) {
		currentFrictionYZ = rotFriction * 5.0f;
	}

	// 慣性
	float frictionT = 1.0f - std::exp(-rotFriction * deltaTime);

	angularVelocity.x = Lerp(angularVelocity.x, 0.0f, frictionT);
	angularVelocity.y = Lerp(angularVelocity.y, 0.0f, frictionT);
	angularVelocity.z = Lerp(angularVelocity.z, 0.0f, frictionT);
	// ▲▲▲ 修正ここまで ▲▲▲

	// 3. 現在の角度に、計算された角速度を加算
	currentPitch += angularVelocity.x * deltaTime;
	currentYaw += angularVelocity.y * deltaTime;
	currentRoll += angularVelocity.z * deltaTime;

	
	// 4. 回転しすぎないように最大角度で制限（クランプ）
	// ※限界角度に達したら、ピタッと止まるようにその軸の角速度も 0 にリセットする
	if (currentPitch <= -maxPitchAngle || currentPitch >= maxPitchAngle) {
		currentPitch = std::clamp(currentPitch, -maxPitchAngle, maxPitchAngle);
		angularVelocity.x = 0.0f;
	}
	if (currentYaw <= -maxYawAngle || currentYaw >= maxYawAngle) {
		currentYaw = std::clamp(currentYaw, -maxYawAngle, maxYawAngle);
		angularVelocity.y = 0.0f;
	}
	if (currentRoll <= -maxRollAngle || currentRoll >= maxRollAngle) {
		currentRoll = std::clamp(currentRoll, -maxRollAngle, maxRollAngle);
		angularVelocity.z = 0.0f;
	}

	// 5. G力による位置ズレの処理（そのまま）
	Vector3 targetOffset = {
		-playerInput.axisX * maxGForceOffset.x,
		-playerInput.axisY * maxGForceOffset.y,
		0.0f
	};
	float posT = 1.0f - std::exp(-positionSpeed * deltaTime);
	currentLocalOffset = Lerp(currentLocalOffset, targetOffset, posT);

	// 6. 最終的な回転の適用
	rotate_.x = baseRot.x + currentPitch; // X軸: ピッチ(上下の傾き)
	rotate_.y = baseRot.y + currentYaw;   // Y軸: ヨー(左右の首振り)
	rotate_.z = baseRot.z + currentRoll;  // Z軸: ロール(左右の傾き)

	// ▼▼▼ 復活：自機の移動処理（慣性あり） ▼▼▼
	// 目標とする速度を算出
	float moveT = 1.0f - std::exp(-inertiaSpeed * deltaTime);
	Vector2 targetVelocity = {
		playerInput.axisX * moveSpeed,
		playerInput.axisY * moveSpeed
	};
	velocity.x = Lerp(velocity.x, targetVelocity.x, moveT);
	velocity.y = Lerp(velocity.y, targetVelocity.y, moveT);

	// 移動量を加算し、画面端に行かないようクランプ
	playerPositionOffset.x += velocity.x * deltaTime;
	playerPositionOffset.y += velocity.y * deltaTime;
	playerPositionOffset.x = std::clamp(playerPositionOffset.x, -moveLimitX, moveLimitX);
	playerPositionOffset.y = std::clamp(playerPositionOffset.y, -moveLimitY, moveLimitY);

	// ローカル移動量 (playerPositionOffset) と G力によるズレ (currentLocalOffset) を合成
	Vector3 combinedOffset = {
		playerPositionOffset.x + currentLocalOffset.x,
		playerPositionOffset.y + currentLocalOffset.y,
		playerPositionOffset.z + currentLocalOffset.z
	};

	// カメラのベース回転を元にワールド座標のズレに変換
	Matrix4x4 matBaseRot = MakeRotateMatrix(baseRot);
	Vector3 worldOffset = VectorTransform(combinedOffset, matBaseRot);

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
			bullet->SetDeltaTime(deltaTime);
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

void Player::UpdateDeath(float deltaTime) {
	// カメラ情報の取得
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 cameraWorld = camera->GetWorldMatrix();

	// カメラの前方ベクトル (ローカルZ軸方向)
	Vector3 forward = {
		cameraWorld.m[2][0],
		cameraWorld.m[2][1],
		cameraWorld.m[2][2]
	};

	// 演出用のパラメータ（必要に応じて数値を調整してください）
	const float forwardSpeed = 10.0f; // 前方へ吹き飛ぶ速度
	const float fallSpeed = 5.0f;    // 下方向に落ちる速度
	float spinSpeed = 5.0f;    // Z軸（ロール）の回転速度

	// カメラ前方方向へ進みつつ、ワールドの下(-Y)へ落ちる
	translate_.x += forward.x * forwardSpeed * deltaTime;
	translate_.y += (forward.y * forwardSpeed - fallSpeed) * deltaTime;
	translate_.z += forward.z * forwardSpeed * deltaTime;

	// Z軸回転（カメラのZ軸回転と同等のロール回転）
	rotate_.x = Lerp(rotate_.x, 0.3f, 0.01f);
	rotate_.z += spinSpeed * deltaTime;

	// Transformへ適用
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform.translate = translate_;
		GetGameObject()->GetTransform()->transform.rotate = rotate_;
		GetGameObject()->GetTransform()->transform.scale = scale_;
	}
}
