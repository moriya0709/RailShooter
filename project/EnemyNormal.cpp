#include "EnemyNormal.h"
#include "CameraManager.h"
#include "EnemyNormalBullet.h"

void EnemyNormal::Initialize() {
	// 3Dオブジェクトの生成
	auto camera = CameraManager::GetInstance()->GetActiveCamera();
	object = std::make_unique<Object>();
	object->Initialize(camera);
	object->SetModel("ball.gltf");

	// 初期位置を保持
	initialPosition = transform.translate;

	object->SetTranslate(transform.translate);
	object->SetRotate(transform.rotate);
	object->SetScale(transform.scale);

}

void EnemyNormal::Update(Vector3 playerPosition, float currentPlayerProgress) {
	// ★デルタタイムの定義（将来的に GamePlayScene から deltaTime を引数で渡す形に拡張推奨）
	float deltaTime = 1.0f / 60.0f;
		aliveTime += deltaTime;

	// 当たり判定リセット
	if (hitTimer_ > 0.0f) {
		hitTimer_ -= deltaTime;
		isHit = true;
	} else {
		isHit = false;
	}

	// =========================================================
	// ★移動パターンの計算（object->Update() より前に実行）[cite: 8]
	// =========================================================
	if (movePattern == "IDRE") {
	}
	if (movePattern == "STRAIGHT") {
		// まっすぐ奥から手前（Z軸マイナス方向）へ進む
		transform.translate.z -= speed * deltaTime;

	} else if (movePattern == "WAVE") {
		// 手前に進みながら、X軸をサイン波で左右に揺らす
		transform.translate.z -= speed * deltaTime;
		transform.translate.x = initialPosition.x + std::sin(aliveTime * 5.0f) * 3.0f;

	} else if (movePattern == "HOMING") {
		// プレイヤーの位置に向かって少しずつ接近する
		Vector3 dir = playerPosition - transform.translate;
		float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
		if (length > 0.001f) {
			dir.x /= length; dir.y /= length; dir.z /= length; // 正規化
			transform.translate.x += dir.x * (speed * 0.5f) * deltaTime;
			transform.translate.y += dir.y * (speed * 0.5f) * deltaTime;
			transform.translate.z += dir.z * (speed * 0.5f) * deltaTime;
		}
	}else if (movePattern == "PATH" && controlPoints.size() >= 2) {
		
		// 進行度を進める
		pathProgress += (speed * 0.2f) * deltaTime;

		int index = static_cast<int>(pathProgress);
		float t = pathProgress - index;

		// パスの終点に達していない場合
		if (index < static_cast<int>(controlPoints.size()) - 1) {

			// スプライン計算に必要な前後の 4 つの制御点インデックスを算出（端のオーバーフロー防止）
			size_t p0_idx = (index == 0) ? 0 : index - 1;
			size_t p1_idx = index;
			size_t p2_idx = index + 1;
			size_t p3_idx = (index + 2 < controlPoints.size()) ? index + 2 : p2_idx;

			// 4つの制御点を取得
			const Vector3& p0 = controlPoints[p0_idx];
			const Vector3& p1 = controlPoints[p1_idx];
			const Vector3& p2 = controlPoints[p2_idx];
			const Vector3& p3 = controlPoints[p3_idx];

			// ★ スプライン曲線上の座標を設定
			transform.translate = CatmullRomSpline(p0, p1, p2, p3, t);

		} else {
			// ★ PATH の終点に到達した時の処理
			if (!railPoints.empty()) {
				// レールデータがあれば RAIL_FORWARD モードへ自動遷移
				movePattern = "RAIL_FORWARD";

			} else {
				// レールデータが無い場合のみ削除
				isDead_ = true;
			}
		}
	} else if (movePattern == "RAIL_FORWARD") {
		if (!railPoints.empty()) {
			// 敵のレール上の進行度 = プレイヤーの進行度 + 敵固有の先行オフセット
			float enemyProgress = currentPlayerProgress + railOffsetProgress;

			// 更に、敵自身をXやYに揺らしたい場合は、ここでオフセットを加算することも可能です
			// float waveOffset = std::sin(aliveTime * 3.0f) * 2.0f;

			// レール上の座標を取得してセット
			Vector3 basePos = GetSplinePosition(railPoints, enemyProgress);

			transform.translate.x = basePos.x;
			transform.translate.y = basePos.y; // ＋ waveOffset (揺らす場合)
			transform.translate.z = basePos.z;
		}
	}

	// 座標変更後に 3D オブジェクトへセットして更新dw
	object->SetTranslate(transform.translate);
	object->SetRotate(transform.rotate);
	object->SetScale(transform.scale);
	object->Update();

	// 毎フレームクールダウンを減らす
	if (shotCoolTime > 0.0f) {
		shotCoolTime -= 1.0f / 60.0f; // 60 FPSを想定
	}

	// 弾の生成
	if (shotCoolTime <= 0) {
		auto bullet = std::make_unique<EnemyNormalBullet>();
		bullet->Initialize(transform.translate, playerPosition);
		bullets_.push_back(std::move(bullet));

		shotCoolTime = 0.5f; // クールタイムをリセット
	}


	// 全ての弾を更新
	for (auto& bullet : bullets_) {
		bullet->Update();
	}

	// デスフラグが立っている弾をリストから一括削除
	bullets_.remove_if([](const std::unique_ptr<EnemyBullet>& bullet) {
		return bullet->IsDead();
		});
}

void EnemyNormal::Draw() {
	object->Draw();
}

Vector3 EnemyNormal::GetSplinePosition(const std::vector<Vector3>& points, float progress) {
	if (points.empty()) return { 0.0f, 0.0f, 0.0f };
	if (points.size() == 1) return points[0];

	// 進行度がオーバーしないように丸める
	float maxProgress = static_cast<float>(points.size() - 1);
	if (progress <= 0.0f) progress = 0.0f;
	if (progress >= maxProgress) progress = maxProgress - 0.001f; // 終点を超えないよう微小値を引く

	int index = static_cast<int>(progress);
	float t = progress - index;

	size_t p0_idx = (index == 0) ? 0 : index - 1;
	size_t p1_idx = index;
	size_t p2_idx = index + 1;
	size_t p3_idx = (index + 2 < points.size()) ? index + 2 : p2_idx;

	return CatmullRomSpline(points[p0_idx], points[p1_idx], points[p2_idx], points[p3_idx], t);
}

void EnemyNormal::OnCollision() {
	hitTimer_ = 0.2f; // 衝突時に0.2秒間（60FPSで約12フレーム）タイマーをセット
}

OBB EnemyNormal::GetOBB() {
	OBB obb;
	// 1. 中心座標
	obb.center = transform.translate;

	// 2. ハーフサイズ（TransformのScaleを適用）
	obb.halfExtents = {
		transform.scale.x * 0.5f,
		transform.scale.y * 0.5f,
		transform.scale.z * 0.5f
	};

	// 3. 回転行列から正規化された3軸を取得 (Rotateから回転行列を作成)
	Matrix4x4 rotMat = MakeRotateMatrix(transform.rotate); // 自身のライブラリの回転行列作成関数[cite: 3]

	obb.axes[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] }; // X軸
	obb.axes[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }; // Y軸
	obb.axes[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] }; // Z軸

	return obb;
}
