#include "PlayerBulletNormal.h"
#include "Camera.h"
#include "TrailEffectManager.h"

void PlayerBulletNormal::Initialize(const Vector3 position) {
	// プレイヤーの座標を代入
	transform.translate = position;

	// カメラの情報を取得して弾の進行方向を設定する
	Camera* camera = Camera::GetInstance(); // シングルトンインスタンスの取得[cite: 4]
	Matrix4x4 cameraWorld = camera->GetWorldMatrix(); // カメラのワールド行列を取得[cite: 4]

	// worldMatrix からカメラの「前方向(Z軸)」ベクトルを抽出[cite: 3]
	Vector3 forward = {
		cameraWorld.m[2][0],
		cameraWorld.m[2][1],
		cameraWorld.m[2][2]
	};

	// 前方向ベクトルにスピードを掛けて、1フレームあたりの速度(移動量)を算出
	velocity.x = forward.x * speed;
	velocity.y = forward.y * speed;
	velocity.z = forward.z * speed;

	// トレイルエフェクトの初期化
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", transform, width, trailMaxLifeTime);
	trailEffect->LoadCsv("Resource/trail/shot.csv");
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

}

void PlayerBulletNormal::Update() {
	// 寿命タイマーを進める
	deathTimer += 1.0f / 60.0f;
	if (deathTimer >= kLifeTime) {
		isDead_ = true; // 寿命が来たら消滅フラグを立てる
	}

	// 計算しておいた速度ベクトルを現在位置に加算して弾を移動させる
	transform.translate.x += velocity.x;
	transform.translate.y += velocity.y;
	transform.translate.z += velocity.z;

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
