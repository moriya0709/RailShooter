#include "EnemyNormalBullet.h"
#include "TrailEffectManager.h"

void EnemyNormalBullet::Initialize(Vector3 position, Vector3 playerPosition) {
	// 弾の初期位置を設定
	transform.translate = position;

    // --- プレイヤーに向かう速度を計算 ---
    // ターゲットへの方向ベクトルを求める (終点 - 始点)
    Vector3 direction = {
        playerPosition.x - position.x,
        playerPosition.y - position.y,
        playerPosition.z - position.z
    };
    // ベクトルを正規化
    direction = Normalize(direction);
    // 速度を決定
    velocity.x = direction.x * speed;
    velocity.y = direction.y * speed;
    velocity.z = direction.z * speed;

	// トレイルエフェクトの初期化
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", transform, width, trailMaxLifeTime);
	trailEffect->LoadCsv("Resource/trail/shot.csv");
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

}

void EnemyNormalBullet::Update() {
    // 弾の移動
    transform.translate.x += velocity.x;
    transform.translate.y += velocity.y;
    transform.translate.z += velocity.z;

    // 寿命の管理
    deathTimer += 1.0f / 60.0f;
    if (deathTimer >= kLifeTime) {
        isDead_ = true; 
    }

    trailEffect->AddPoint(transform.translate);
    trailEffect->SetTranslate(transform.translate);
}

OBB EnemyNormalBullet::GetOBB() {
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
