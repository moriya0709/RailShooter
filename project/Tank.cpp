#include "Tank.h"
#include "CameraManager.h"
#include "TankBullet.h"
#include "GameObject.h"
#include "ModelRendererComponent.h"
#include "ScoreManager.h"
#include "EasingComponent.h"
#include "SceneManager.h"
#include "HitStopManager.h"
#include "SpawnedVisualPartsComponent.h"

#include <limits>

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

void Tank::Initialize() {
	// 部位パーツ
	if (GetGameObject()) {
		if (auto* visualParts = GetGameObject()->GetComponent<SpawnedVisualPartsComponent>()) {
			isBodyPart_ = visualParts->GetPartLocalTransform("Body", transform);
			isRackPart_ = visualParts->GetPartLocalTransform("Rack", rackTransform);
			isBarrelPart_ = visualParts->GetPartLocalTransform("Barrel", barrelTransform);
		}
	}

	// 初期位置を保持
	initialPosition = transform.translate;
	hasPreviousPathYaw_ = false;
	BuildPathRoute();
}

void Tank::SetControlPoints(const std::vector<Vector3>& points) {
	controlPoints = points;
	pathProgress = 0.0f;
	pathReachedEnd_ = false;
	hasPreviousPathYaw_ = false;
	BuildPathRoute();
}

void Tank::BuildPathRoute() {
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

void Tank::Update(Vector3 playerPosition, float currentPlayerProgress) {
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

				// コンボリセット
				ScoreManager::GetInstance()->ResetCombo();
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
	// 向き・傾きの計算
	// =========================================================

	// 進行方向の先読み
	Vector3 playerVelocity{};
	if (hasPreviousPlayerPosition_ && deltaTime > 0.0001f) {
		playerVelocity = {
			(playerPosition.x - previousPlayerPosition_.x) / deltaTime,
			(playerPosition.y - previousPlayerPosition_.y) / deltaTime,
			(playerPosition.z - previousPlayerPosition_.z) / deltaTime
		};
	}

	const Vector3 aimTarget = {
		playerPosition.x + playerVelocity.x * leadTime_,
		playerPosition.y + playerVelocity.y * leadTime_,
		playerPosition.z + playerVelocity.z * leadTime_
	};

	// 砲台の向き
	Vector3 dirToPlayer = {
		aimTarget.x - transform.translate.x,
		aimTarget.y - transform.translate.y,
		aimTarget.z - transform.translate.z
	};

	const float distance = std::sqrt(
		dirToPlayer.x * dirToPlayer.x +
		dirToPlayer.y * dirToPlayer.y +
		dirToPlayer.z * dirToPlayer.z);

	if (distance > 0.001f) {
		dirToPlayer.x /= distance;
		dirToPlayer.y /= distance;
		dirToPlayer.z /= distance;

		// 銃架の回転
		rackTransform.rotate.y = std::atan2(dirToPlayer.x, dirToPlayer.z);

		// 銃身の回転
		const float horizontalDistance = std::sqrt(dirToPlayer.x * dirToPlayer.x + dirToPlayer.z * dirToPlayer.z);
		barrelTransform.rotate.x = std::atan2(dirToPlayer.y, horizontalDistance);
		barrelTransform.rotate.y = rackTransform.rotate.y;

	}

	// 座標更新
	if (GetGameObject()) {
		if (auto* visualParts = GetGameObject()->GetComponent<SpawnedVisualPartsComponent>()) {
			if (isBodyPart_) {
				visualParts->SetPartLocalTransform("Body", transform);
			}
			if (isRackPart_) {
				visualParts->SetPartLocalTransform("Rack", rackTransform);
			}
			if (isBarrelPart_) {
				visualParts->SetPartLocalTransform("Barrel", barrelTransform);
			}
		}
	}

	// =========================================================
	// 弾の発射処理（元のコードのまま）
	// =========================================================
	// 毎フレームクールダウンを減らす
	if (shotCoolTime > 0.0f) {
		shotCoolTime -= deltaTime;
	}

	// 弾の生成
	if (isShot) {
		// ターゲットまでの距離を計算
		Vector3 toTarget = {
			aimTarget.x - transform.translate.x,
			aimTarget.y - transform.translate.y,
			aimTarget.z - transform.translate.z
		};
		float targetDist = std::sqrt(
			toTarget.x * toTarget.x +
			toTarget.y * toTarget.y +
			toTarget.z * toTarget.z
		);

		// ターゲット地点でのブレ幅（半径）を算出
		const float spreadRad = spreadAngleDeg_ * (3.14159265f / 180.0f);
		const float spreadRadius = targetDist * std::tan(spreadRad);

		// ランダムなオフセットを生成
		std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * 3.14159265f);
		std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);

		float theta = distAngle(randomEngine_);
		float r = spreadRadius * std::sqrt(distRadius(randomEngine_)); // sqrtで中心への偏りを防止

		// ターゲット位置にブレを加算
		Vector3 spreadTarget = aimTarget;
		spreadTarget.x += std::cos(theta) * r;
		spreadTarget.y += std::sin(theta) * r;

		// 弾の生成
		auto bulletObject = std::make_unique<GameObject>("EnemyBullet");
		auto bullet = bulletObject->AddComponent<TankBullet>();
		bullet->Initialize(transform.translate, spreadTarget);
		bulletObject->Initialize();
		bullets_.push_back(bullet);
		bulletObjects_.push_back(std::move(bulletObject));

		shotCoolTime = kShotCoolTime; // クールタイムをリセット
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


	previousPlayerPosition_ = playerPosition;
	hasPreviousPlayerPosition_ = true;
}

void Tank::Draw() {
	// ModelRendererComponent が GameObject::Draw() で描画する。
}

Vector3 Tank::GetSplinePosition(const std::vector<Vector3>& points, float progress) {
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

void Tank::OnCollisionBullet(int damage) {
	hitTimer_ = 0.2f; // 衝突時に0.2秒間（60FPSで約12フレーム）タイマーをセット

	hp -= damage;
	if (hp <= 0) {
		isDead_ = true;
		isDefeated_ = true;

		// キルの速度に応じた評価を設定
		if (aliveTime < kRatingChangeTime[0]) {
			ScoreManager::GetInstance()->SetRating("Perfect");
		} else if (aliveTime < kRatingChangeTime[1]) {
			ScoreManager::GetInstance()->SetRating("Great");
		} else if (aliveTime < kRatingChangeTime[2]) {
			ScoreManager::GetInstance()->SetRating("Good");
		} else {
			ScoreManager::GetInstance()->SetRating("Normal");
		}

		// スコア加算(死亡時)
		ScoreManager::GetInstance()->AddScore(scoreValueDeath, 1);
		// コンボ加算
		ScoreManager::GetInstance()->AddCombo();
		// ヒットストップ
		HitStopManager::GetInstance()->Request(20, 0.25f);

		// スコア演出
		if (auto* comboObject = GameObject::FindByName("ComboText")) {
			comboObject->SetActive(true);
			if (auto* comboEasing = comboObject->GetComponent<EasingComponent>()) {
				comboEasing->Play(0);
			}
		}
		if (auto* comboValueObject = GameObject::FindByName("ComboValueText")) {
			comboValueObject->SetActive(true);
			if (auto* comboValueEasing = comboValueObject->GetComponent<EasingComponent>()) {
				comboValueEasing->Play(0);
			}
		}
		if (auto* killScoreObject = GameObject::FindByName("KillScore")) {
			killScoreObject->SetActive(true);
			if (auto* killScoreEasing = killScoreObject->GetComponent<EasingComponent>()) {
				killScoreEasing->Play(0);
			}
		}
		if (auto* ratingObject = GameObject::FindByName("Rating")) {
			ratingObject->SetActive(true);
			if (auto* ratingEasing = ratingObject->GetComponent<EasingComponent>()) {
				ratingEasing->Play(0);
			}
		}
		if (auto* ratingScoreObject = GameObject::FindByName("RatingScore")) {
			ratingScoreObject->SetActive(true);
			if (auto* ratingScoreEasing = ratingScoreObject->GetComponent<EasingComponent>()) {
				ratingScoreEasing->Play(0);
			}
		}



	} else {
		// スコア加算(ヒット時)
		ScoreManager::GetInstance()->AddScore(scoreValueHit);
	}
}

OBB Tank::GetOBB() {
	OBB obb;
	// 1. 中心座標
	obb.center = transform.translate;

	// 2. ハーフサイズ（TransformのScaleを適用）
	obb.halfExtents = {
		hitRadius.x,
		hitRadius.y,
		hitRadius.z
	};

	// 3. 回転行列から正規化された3軸を取得 (Rotateから回転行列を作成)
	Matrix4x4 rotMat = MakeRotateMatrix(transform.rotate); // 自身のライブラリの回転行列作成関数[cite: 3]

	obb.axes[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] }; // X軸
	obb.axes[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }; // Y軸
	obb.axes[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] }; // Z軸

	return obb;
}
