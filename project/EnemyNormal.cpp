#include "EnemyNormal.h"
#include "CameraManager.h"
#include "EnemyNormalBullet.h"
#include "GameObject.h"
#include "ModelRendererComponent.h"

namespace {

// RailCamera::Evaluate と同じ区間定義で Catmull-Rom を評価する。
// レールカメラは t=0 で points[1] にいるため、通常のパス補間
// （t=0 で points[0]）を使うと敵がカメラの後方へずれてしまう。
Vector3 EvaluateRailPath(const std::vector<Vector3>& points, float railT) {
	if (points.size() < 4) {
		return { 0.0f, 0.0f, 0.0f };
	}

	const float maxT = static_cast<float>(points.size() - 3);
	railT = std::clamp(railT, 0.0f, maxT - 0.001f);
	const size_t index = static_cast<size_t>(railT);
	const float localT = railT - static_cast<float>(index);

	return CatmullRomSpline(
		points[index], points[index + 1], points[index + 2], points[index + 3], localT);
}

} // namespace

void EnemyNormal::Initialize() {
	// 描画・Transform は同じ GameObject のコンポーネントに委譲する。
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform = transform;
		auto* renderer = GetGameObject()->GetComponent<ModelRendererComponent>();
		if (!renderer) {
			renderer = GetGameObject()->AddComponent<ModelRendererComponent>();
			renderer->SetModel("ball.gltf");
		}
	}

	// 初期位置を保持
	initialPosition = transform.translate;
	BuildPathRoute();
}

void EnemyNormal::SetControlPoints(const std::vector<Vector3>& points) {
	controlPoints = points;
	pathProgress = 0.0f;
	pathReachedEnd_ = false;
	BuildPathRoute();
}

void EnemyNormal::BuildPathRoute() {
	pathRoutePoints = controlPoints;
	if (pathRoutePoints.empty()) {
		return;
	}

	// スポーン地点が最初の制御点と異なる場合、そこから最初の制御点までを
	// PATH の最初の区間として追加する。これにより開始位置への瞬間移動を防ぐ。
	const Vector3 delta = transform.translate - pathRoutePoints.front();
	const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
	if (distanceSquared > 0.000001f) {
		pathRoutePoints.insert(pathRoutePoints.begin(), transform.translate);
	}
}

void EnemyNormal::Update(Vector3 playerPosition, float currentPlayerProgress) {
	const float deltaTime = deltaTime_;
	aliveTime += deltaTime;

	// 当たり判定リセット
	if (hitTimer_ > 0.0f) {
		hitTimer_ -= deltaTime;
		isHit = true;
	} else {
		isHit = false;
	}

	// =========================================================
	// ★移動パターンの計算
	// =========================================================

	// 移動前の座標を保持
	Vector3 oldPos = transform.translate;

	if (movePattern == "IDRE") {
	} else if (movePattern == "STRAIGHT") {
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
	} else if (movePattern == "PATH" && pathRoutePoints.size() >= 2) {

		const float finalProgress = static_cast<float>(pathRoutePoints.size() - 1);
		if (pathReachedEnd_) {
			// 最後の制御点を少なくとも 1 フレーム描画してから従来どおり終了する。
			if (!railPoints.empty()) {
				isDead_ = true;
			}
		} else {
			const float previousProgress = pathProgress;
			pathProgress += (speed * pathSpeed) * deltaTime;

			// フレーム間で制御点をまたいだ場合も、その点を一度は正確に通過する。
			const int previousSection = static_cast<int>(previousProgress);
			const int currentSection = static_cast<int>(pathProgress);
			if (currentSection > previousSection && currentSection < static_cast<int>(finalProgress)) {
				pathProgress = static_cast<float>(currentSection);
			}

			if (pathProgress >= finalProgress) {
				pathProgress = finalProgress;
				transform.translate = pathRoutePoints.back();
				pathReachedEnd_ = true;
			} else {
				const int index = static_cast<int>(pathProgress);
				const float t = pathProgress - static_cast<float>(index);

				// スプライン計算に必要な前後の 4 つの制御点インデックスを算出（端のオーバーフロー防止）
				size_t p0_idx = (index == 0) ? 0 : index - 1;
				size_t p1_idx = index;
				size_t p2_idx = index + 1;
				size_t p3_idx = (index + 2 < pathRoutePoints.size()) ? index + 2 : p2_idx;

				// 4つの制御点を取得
				const Vector3& p0 = pathRoutePoints[p0_idx];
				const Vector3& p1 = pathRoutePoints[p1_idx];
				const Vector3& p2 = pathRoutePoints[p2_idx];
				const Vector3& p3 = pathRoutePoints[p3_idx];

				// ★ スプライン曲線上の座標を設定
				transform.translate = CatmullRomSpline(p0, p1, p2, p3, t);
			}
		}
	} else if (movePattern == "RAIL_FORWARD") {
		if (!railPoints.empty()) {
			float enemyProgress = currentPlayerProgress + railOffsetProgress;
			Vector3 basePos = EvaluateRailPath(railPoints, enemyProgress);

			transform.translate.x = basePos.x;
			transform.translate.y = basePos.y;
			transform.translate.z = basePos.z;
		}
	}

	// =========================================================
	// ★向き・傾きの計算
	// =========================================================
	// 移動前後の差分から進行方向(velocity)を求める
	Vector3 velocity = {
		transform.translate.x - oldPos.x,
		transform.translate.y - oldPos.y,
		transform.translate.z - oldPos.z
	};

	// 移動している場合のみ向きを更新
	float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
	if (speedSq > 0.000001f) {
		// Y軸回転 (Yaw: 左右の向き) 
		transform.rotate.y = std::atan2(velocity.x, velocity.z);

		// X軸回転 (Pitch: 上下の向き) 
		float xzLen = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
		transform.rotate.x = std::atan2(-velocity.y, xzLen);

		// Z軸回転 (Roll: 機体の傾き)
		if (movePattern == "PATH") {
			// 今回のX軸方向への移動量をベースに傾きを計算
			float rollSensitivity = 2.0f; // 感度調整
			float maxRollAngle = 1.0f;    // 最大角（ラジアン）

			// 右に動いたとき(velocity.x > 0)はマイナス方向に回転させたいので符号を反転
			float targetRoll = -velocity.x * rollSensitivity;
			targetRoll = std::clamp(targetRoll, -maxRollAngle, maxRollAngle);

			float lerpSpeed = 0.1f;
			transform.rotate.z = std::lerp(transform.rotate.z, targetRoll, lerpSpeed);
		} else {
			// PATH以外の動きの場合は水平(0.0f)に戻す
			transform.rotate.z = std::lerp(transform.rotate.z, 0.0f, 0.1f);
		}
	}

	// 座標変更後に 3D オブジェクトへセットして更新
	if (GetGameObject()) {
		GetGameObject()->GetTransform()->transform = transform;
	}

	// =========================================================
	// ★弾の発射処理（元のコードのまま）
	// =========================================================
	// 毎フレームクールダウンを減らす
	if (shotCoolTime > 0.0f) {
		shotCoolTime -= deltaTime;
	}

	// 弾の生成
	if (shotCoolTime <= 0) {
		auto bulletObject = std::make_unique<GameObject>("EnemyBullet");
		auto bullet = bulletObject->AddComponent<EnemyNormalBullet>();
		bullet->Initialize(transform.translate, playerPosition);
		bulletObject->Initialize();
		bullets_.push_back(bullet);
		bulletObjects_.push_back(std::move(bulletObject));

		shotCoolTime = 0.5f; // クールタイムをリセット
	}

	// 全ての弾を更新
	for (auto& bulletObject : bulletObjects_) {
		if (auto bullet = bulletObject->GetComponent<EnemyBullet>()) {
			bullet->SetDeltaTime(deltaTime);
		}
		bulletObject->Update();
	}

	bullets_.remove_if([](EnemyBullet* bullet) { return bullet->IsDead(); });	
	bulletObjects_.remove_if([](const std::unique_ptr<GameObject>& bulletObject) {
		auto bullet = bulletObject->GetComponent<EnemyBullet>();
		return bullet && bullet->IsDead();
	});
}

void EnemyNormal::Draw() {
	// ModelRendererComponent が GameObject::Draw() で描画する。
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

void EnemyNormal::OnCollisionBullet(int damage) {
	hitTimer_ = 0.2f; // 衝突時に0.2秒間（60FPSで約12フレーム）タイマーをセット

	hp -= damage;
	if (hp <= 0) {
		isDead_ = true;
	}
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
