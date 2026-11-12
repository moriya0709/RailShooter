#pragma once
#include <memory>
#include <utility>
#include <list>
#include <string>

#include "Enemy.h"
#include "Calc.h"
#include "EnemyBullet.h"
#include "CommonStructs.h"
#include <vector>
#include "CollisionManager.h"
#include "GameObject.h"

class EnemyNormal : public Enemy {
public:
	void Initialize() override;
	void Update(Vector3 playerPosition, float currentPlayerProgress) override;
	void SetDeltaTime(float deltaTime) override { deltaTime_ = (std::max)(0.0f, deltaTime); }
	void Draw() override;

	// 指定した制御点リスト上の座標を算出する関数
	Vector3 GetSplinePosition(const std::vector<Vector3>& points, float progress) override;

	void SetTransform(const Transform& transform) override { this->transform = transform; }

	Vector3 GetTranslate() const override { return transform.translate; }
	void OnCollisionBullet(int damage) override;
	bool IsDead() const override { return isDead_; }
	bool IsHit() const override { return isHit; }

	// 移動パターンの設定
	void SetMovePattern(const std::string& pattern) override { movePattern = pattern; }
	void SetPathSpeed(float value) override { pathSpeed = (std::max)(0.0f, value); }
	// 制御点の設定
	void SetControlPoints(const std::vector<Vector3>& points) override;
	// レール追従用の制御点をセット
	void SetRailPoints(const std::vector<Vector3>& points) override { railPoints = points; }
	// レール追従用のオフセットをセットd
	void SetRailOffsetProgress(float offset) override { railOffsetProgress = offset; }

	const std::list<EnemyBullet*>& GetBullets() override { return bullets_; }

	// OBBを取得
	OBB GetOBB() override;

public:
	enum State {
		Idle,
		Move,
		Attack,
		Death
	};
	State currentState = Idle;

	Transform transform{
		{ 1.0f, 1.0f, 1.0f }, // scale
		{ 0.0f, 0.0f, 0.0f }, // rotate
		{ 0.0f, 0.0f, 0.0f }  // translate
	};

	bool isDead_ = false;

	// 移動
	std::string movePattern = "STRAIGHT"; // デフォルトの移動パターン
	float aliveTime = 0.0f;                   // 経過時間
	float speed = 8.0f;                       // 移動速度
	Vector3 initialPosition{ 0.0f, 0.0f, 0.0f }; // スポーン時の初期座標
	// 制御点
	std::vector<Vector3> controlPoints; // 辿るべき座標のリスト
	// スポーン地点を必要に応じて先頭に加えた、実際に辿る PATH 用の点列。
	std::vector<Vector3> pathRoutePoints;
	float pathSpeed = 0.125f;
	float pathProgress = 0.0f;          // 現在の進行度 (0.0 ～ ポイント数-1)
	bool pathReachedEnd_ = false;
	// 曲線追従中の旋回量から、曲線の内側へバンクさせるための前フレームの方位。
	float previousPathYaw_ = 0.0f;
	bool hasPreviousPathYaw_ = false;
	// レール追従(RAIL_FORWARD)用の変数
	std::vector<Vector3> railPoints;    // レールカメラの制御点リスト
	// 既存の短い先行距離を維持し、視野内でレール追従させる。
	float railOffsetProgress = 0.05f;
	Vector3 previousPosition{ 0.0f, 0.0f, 0.0f }; // 前フレームの座標
	const float kBankSensitivity = 2.0f; // バンク角の感度（値が大きいほど旋回に対してバンクが大きくなる）
	const float kMaxBankAngle = 1.57f; // カーブ時の最大傾き角

	// 弾
	std::list<std::unique_ptr<GameObject>> bulletObjects_;
	std::list<EnemyBullet*> bullets_;
	float shotCoolTime = 20.0f; // 射撃のクールタイム

	// 当たったか
	bool isHit = false;
	float hitTimer_ = 0.0f;
	float deltaTime_ = 1.0f / 60.0f;
	Vector3 hitRadius = { 0.5f, 0.5f, 0.5f }; // 当たり判定の半径

	// キル速度の評価変化時間
	std::vector<float> kRatingChangeTime = { 5.0f, 10.0f, 15.0f };

	// HP
	int hp = 10;

	// スコア値
	float scoreValueHit = 100.0f; // ヒット時
	float scoreValueDeath = 3000.0f; // 死亡時

	void BuildPathRoute();
};

