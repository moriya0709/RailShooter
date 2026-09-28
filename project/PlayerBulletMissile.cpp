#include "PlayerBulletMissile.h"
#include "Camera.h"
#include "CameraManager.h"
#include "TrailEffectManager.h"
#include "Enemy.h"
#include "GameObject.h"


void PlayerBulletMissile::Initialize(const Vector3 position, Enemy* target, const Vector3& inheritedVelocity) {
	// 代入
	transform.translate = position;
	previousTranslate_ = position;
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform = transform;
	}
	centerPos = position; // 中心軸の初期位置
	target_ = target;
	inheritedVelocity_ = inheritedVelocity;

	// 通常弾と同様に、レール移動とピッチを反映したアクティブカメラを使う。
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 cameraWorld = camera->GetWorldMatrix();

	// カメラの前方向(Z軸)を抽出
	Vector3 forward = { cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] };

	// 前方向ベクトルを正規化してスピードを掛け、まっすぐ飛ばす
	float fLen = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (fLen > 0.001f) {
		projectileVelocity_.x = (forward.x / fLen) * speed;
		projectileVelocity_.y = (forward.y / fLen) * speed;
		projectileVelocity_.z = (forward.z / fLen) * speed;
	}
	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// パーティクルの初期化
	particle = std::make_unique<ParticleEmitter>();
	particle->Initialize("obj", transform, 1, 0.5f);
	particle->SetActive("obj");
	particle->LoadParticle("Resource/particle/missile.csv");
}

void PlayerBulletMissile::Update() {
	previousTranslate_ = transform.translate;
	// 寿命タイマーを進める
	deathTimer += 1.0f / 60.0f;
	if (deathTimer >= kLifeTime) {
		isDead_ = true;
	}

	// ▼ 追加: 現在の螺旋半径（デフォルトは元のサイズ）
	float currentRadius = spiralRadius;

	// 1. ロックオン対象に向かって中心軸 (centerPos) の進行方向を変更
	if (target_ && !target_->IsDead()) {
		Vector3 dir = target_->GetTranslate() - centerPos;
		float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

		if (length > 0.001f) {
			// ★ 修正1: 敵に近づいたら螺旋の半径を絞って確実に命中させる
			if (length < 15.0f) {
				currentRadius = spiralRadius * (length / 15.0f); // 距離に応じて半径を0に近づける
			}

			dir.x /= length; dir.y /= length; dir.z /= length;

			Vector3 currentDir = { projectileVelocity_.x / speed, projectileVelocity_.y / speed, projectileVelocity_.z / speed };

			float dot = currentDir.x * dir.x + currentDir.y * dir.y + currentDir.z * dir.z;

			// 少し背後に回られても諦めないように条件を緩和 (-0.5f以下で諦める)
			if (dot < -0.5f) {
				target_ = nullptr;
			} else {
				// ★ 修正2: 距離に関係なくホーミングさせる（遠い時は弱く、30以内で強く）
				float currentHomingPower = (length <= homingDistance) ? 0.3f : 0.05f;

				Vector3 newDir;
				newDir.x = Lerp(currentDir.x, dir.x, currentHomingPower);
				newDir.y = Lerp(currentDir.y, dir.y, currentHomingPower);
				newDir.z = Lerp(currentDir.z, dir.z, currentHomingPower);

				float newLen = std::sqrt(newDir.x * newDir.x + newDir.y * newDir.y + newDir.z * newDir.z);
				if (newLen > 0.001f) {
					projectileVelocity_.x = (newDir.x / newLen) * speed;
					projectileVelocity_.y = (newDir.y / newLen) * speed;
					projectileVelocity_.z = (newDir.z / newLen) * speed;
				}
			}
		}
	} else {
		target_ = nullptr;
	}
	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// 2. 中心軸を速度に従って移動
	centerPos.x += velocity.x;
	centerPos.y += velocity.y;
	centerPos.z += velocity.z;

	// 3. 進行方向（velocity）から垂直な右軸(Right)と上軸(Up)を算出
	// ... (この部分は変更なしのため省略) ...
	const float velocityLength = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
	Vector3 forward = velocityLength > 0.001f
		? Vector3{ velocity.x / velocityLength, velocity.y / velocityLength, velocity.z / velocityLength }
		: Vector3{ 0.0f, 0.0f, 1.0f };
	Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	if (std::abs(forward.y) > 0.99f) { worldUp = { 1.0f, 0.0f, 0.0f }; }

	Vector3 right = {
		worldUp.y * forward.z - worldUp.z * forward.y,
		worldUp.z * forward.x - worldUp.x * forward.z,
		worldUp.x * forward.y - worldUp.y * forward.x
	};
	float rLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	if (rLen > 0.001f) { right.x /= rLen; right.y /= rLen; right.z /= rLen; }

	Vector3 up = {
		forward.y * right.z - forward.z * right.y,
		forward.z * right.x - forward.x * right.z,
		forward.x * right.y - forward.y * right.x
	};

	// 4. 螺旋（トルネード）回転のオフセット計算
	// ※前回の「spiralDirection」を反映しています
	spiralAngle += spiralSpeed * spiralDirection * (1.0f / 60.0f);
	float cosA = std::cos(spiralAngle);
	float sinA = std::sin(spiralAngle);

	// ★ 修正3: spiralRadius ではなく currentRadius を掛ける
	Vector3 offset = {
		(right.x * cosA + up.x * sinA) * currentRadius,
		(right.y * cosA + up.y * sinA) * currentRadius,
		(right.z * cosA + up.z * sinA) * currentRadius
	};

	// 5. 最終位置を適用
	Vector3 prevPos = transform.translate;
	transform.translate.x = centerPos.x + offset.x;
	transform.translate.y = centerPos.y + offset.y;
	transform.translate.z = centerPos.z + offset.z;
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform = transform;
	}

	// パーティクルの更新
	particle->SetTranslate(transform.translate);
	particle->Update();
}

OBB PlayerBulletMissile::GetOBB() {
	OBB obb;
	// 1. 中心座標[cite: 3]
	obb.center = transform.translate;

	// 2. ハーフサイズ（TransformのScaleを適用）[cite: 3]
	obb.halfExtents = {
		transform.scale.x * 0.5f,
		transform.scale.y * 0.5f,
		transform.scale.z * 0.5f
	};

	// 3. 回転行列から正規化された3軸を取得 (Rotateから回転行列を作成)[cite: 3]
	Matrix4x4 rotMat = MakeRotateMatrix(transform.rotate); // 自身のライブラリの回転行列作成関数[cite: 3]

	obb.axes[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] }; // X軸
	obb.axes[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }; // Y軸
	obb.axes[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] }; // Z軸

	return obb;
}

void PlayerBulletMissile::RemoveTarget(const Enemy* enemy) {
	if (target_ == enemy) {
		target_ = nullptr;
	}
}

void PlayerBulletMissile::SetInitAngle(bool isRight) {
	spiralAngle = isRight ? 0.0f : 3.14159265f;
}
