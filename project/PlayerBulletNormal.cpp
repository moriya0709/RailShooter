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
		GetGameObject()->GetTransform()->transform = transform;
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

	// スケールの影響を受けないよう正規化して、速度を掛ける。
	const float forwardLength = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (forwardLength > 0.001f) {
		projectileVelocity_.x = (forward.x / forwardLength) * speed;
		projectileVelocity_.y = (forward.y / forwardLength) * speed;
		projectileVelocity_.z = (forward.z / forwardLength) * speed;
	}
	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// トレイルエフェクトの初期化
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", transform, width, trailMaxLifeTime);
	trailEffect->LoadCsv("Resource/trail/playerShot.csv");
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

}

void PlayerBulletNormal::Update() {
	previousTranslate_ = transform.translate;
	// 寿命タイマーを進める
	deathTimer += 1.0f / 60.0f;
	if (deathTimer >= kLifeTime) {
		isDead_ = true; // 寿命が来たら消滅フラグを立てる
	}

	// ロックオン対象に誘導処理
	if (target_ && !target_->IsDead()) {
		Vector3 dir = target_->GetTranslate() - transform.translate;
		float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

		if (length > 0.001f) {
			// 1. ターゲットへの方向ベクトルを正規化
			dir.x /= length; dir.y /= length; dir.z /= length;

			// 2. 誘導の強さ（0.01f ～ 0.1f 程度がおすすめ。高すぎると往復します）
			float homingPower = 0.05f;

			// 3. 現在の速度にターゲット方向の力を加算
			projectileVelocity_.x += dir.x * homingPower;
			projectileVelocity_.y += dir.y * homingPower;
			projectileVelocity_.z += dir.z * homingPower;

			// 4. 加算後の速度ベクトルを再度正規化し、元の speed を掛けて速さを一定に保つ
			float vLen = std::sqrt(projectileVelocity_.x * projectileVelocity_.x + projectileVelocity_.y * projectileVelocity_.y + projectileVelocity_.z * projectileVelocity_.z);
			if (vLen > 0.001f) {
				projectileVelocity_.x = (projectileVelocity_.x / vLen) * speed;
				projectileVelocity_.y = (projectileVelocity_.y / vLen) * speed;
				projectileVelocity_.z = (projectileVelocity_.z / vLen) * speed;
			}
		}
	} else {
		// 敵が死んだ場合はターゲットを外して直進させる
		target_ = nullptr;
	}
	velocity.x = projectileVelocity_.x + inheritedVelocity_.x;
	velocity.y = projectileVelocity_.y + inheritedVelocity_.y;
	velocity.z = projectileVelocity_.z + inheritedVelocity_.z;

	// 計算しておいた速度ベクトルを現在位置に加算して弾を移動させる
	transform.translate.x += velocity.x;
	transform.translate.y += velocity.y;
	transform.translate.z += velocity.z;
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform = transform;
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
