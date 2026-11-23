#include "PlayerBulletNormal.h"
#include "Camera.h"
#include "CameraManager.h"
#include "TrailEffectManager.h"
#include "Enemy.h"
#include "GameObject.h"

void PlayerBulletNormal::Initialize(const Vector3 position, Enemy* target, const Vector3& inheritedVelocity) {
	// 代入
	transform.translate = position;
	previousTranslate_ = position;
	if (GetGameObject()) {
		GetGameObject()->GetTransformComponent()->SetTransform(transform);
	}
	target_ = target;
	inheritedVelocity_ = inheritedVelocity;

	// 実際に描画・操作に使われているカメラを使う。
	// デフォルトカメラではなく、レールカメラのピッチを反映する。
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 cameraWorld = camera->GetWorldMatrix();

	// worldMatrix からカメラの「前方向(Z軸)」ベクトルを抽出[cite: 3]
	Vector3 forward = {
		cameraWorld.m[2][0],
		cameraWorld.m[2][1],
		cameraWorld.m[2][2]
	};

	// 発射時からロックオン対象へ向けることで、ホーミングを即時に開始する。
	if (target_ && !target_->IsDead()) {
		forward = target_->GetTransform().translate - position;
	}

	// スケールの影響を受けないよう正規化して、拡散を適用してから速度を掛ける。
	const float forwardLength = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (forwardLength > 0.001f) {
		Vector3 dir = {
			forward.x / forwardLength,
			forward.y / forwardLength,
			forward.z / forwardLength
		};

		// ★追加：拡散処理
		if (spreadAngleDeg_ > 0.0f) {
			static std::mt19937 rng{ std::random_device{}() };
			std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * 3.14159265f);
			std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);

			// 進行方向 dir に直交する2軸（right, up）を作成
			Vector3 worldUp = (std::abs(dir.y) < 0.99f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
			Vector3 right = {
				worldUp.y * dir.z - worldUp.z * dir.y,
				worldUp.z * dir.x - worldUp.x * dir.z,
				worldUp.x * dir.y - worldUp.y * dir.x
			};
			const float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
			if (rightLen > 0.001f) {
				right.x /= rightLen;
				right.y /= rightLen;
				right.z /= rightLen;
			}

			Vector3 up = {
				dir.y * right.z - dir.z * right.y,
				dir.z * right.x - dir.x * right.z,
				dir.x * right.y - dir.y * right.x
			};

			// 拡散角度から円錐の広がり幅を算出してランダムに散らす
			const float spreadRad = spreadAngleDeg_ * (3.14159265f / 180.0f);
			const float maxRadius = std::tan(spreadRad);
			const float theta = distAngle(rng);
			const float r = maxRadius * std::sqrt(distRadius(rng));

			dir.x += (right.x * std::cos(theta) + up.x * std::sin(theta)) * r;
			dir.y += (right.y * std::cos(theta) + up.y * std::sin(theta)) * r;
			dir.z += (right.z * std::cos(theta) + up.z * std::sin(theta)) * r;

			// 再度正規化
			const float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
			if (dirLen > 0.001f) {
				dir.x /= dirLen;
				dir.y /= dirLen;
				dir.z /= dirLen;
			}
		}

		projectileVelocity_.x = dir.x * speed;
		projectileVelocity_.y = dir.y * speed;
		projectileVelocity_.z = dir.z * speed;
	}
	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// トレイルエフェクトの初期化
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", transform, width, trailMaxLifeTime);
	trailEffect->LoadCsv("Resource/trail/shot.csv");
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

}

void PlayerBulletNormal::Update() {
	const float frameScale = GetDeltaTime() * 60.0f;
	if (frameScale <= 0.0f) {
		return;
	}
	// 寿命タイマーを進める
	previousTranslate_ = transform.translate;
	deathTimer += GetDeltaTime();
	if (deathTimer >= kLifeTime) {
		SetDead(true); // 寿命が来たら消滅フラグを立てる
	}

	// 発射直後から毎フレーム、ロックオン対象へ進行方向を補正する。
	if (target_ && !target_->IsDead()) {
		Vector3 dir = target_->GetTransform().translate - transform.translate;
		float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

		if (length > 0.001f) {
			dir.x /= length;
			dir.y /= length;
			dir.z /= length;

			const float currentHomingPower = homingPower * frameScale;
			projectileVelocity_.x += dir.x * currentHomingPower;
			projectileVelocity_.y += dir.y * currentHomingPower;
			projectileVelocity_.z += dir.z * currentHomingPower;

			const float velocityLength = std::sqrt(
				projectileVelocity_.x * projectileVelocity_.x +
				projectileVelocity_.y * projectileVelocity_.y +
				projectileVelocity_.z * projectileVelocity_.z);
			if (velocityLength > 0.001f) {
				projectileVelocity_.x = (projectileVelocity_.x / velocityLength) * speed;
				projectileVelocity_.y = (projectileVelocity_.y / velocityLength) * speed;
				projectileVelocity_.z = (projectileVelocity_.z / velocityLength) * speed;
			}
		}
	} else {
		target_ = nullptr;
	}

	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// 計算しておいた速度ベクトルを現在位置に加算して弾を移動させる
	transform.translate.x += velocity.x * frameScale;
	transform.translate.y += velocity.y * frameScale;
	transform.translate.z += velocity.z * frameScale;
	if (GetGameObject()) {
		GetGameObject()->GetTransformComponent()->SetTransform(transform);
	}

	trailEffect->AddPoint(transform.translate);
	trailEffect->SetTranslate(transform.translate);
}

OBB PlayerBulletNormal::GetOBB() {
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

void PlayerBulletNormal::RemoveTarget(const Enemy* enemy) {
	if (target_ == enemy) {
		target_ = nullptr;
	}
}
