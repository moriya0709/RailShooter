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
	// 制御点の設定
	void SetControlPoints(const std::vector<Vector3>& points) override { controlPoints = points; }
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
	float pathProgress = 0.0f;          // 現在の進行度 (0.0 ～ ポイント数-1)
	// レール追従(RAIL_FORWARD)用の変数
	std::vector<Vector3> railPoints;    // レールカメラの制御点リスト
	// 既存の短い先行距離を維持し、視野内でレール追従させる。
	float railOffsetProgress = 0.05f;

	// 弾
	std::list<std::unique_ptr<GameObject>> bulletObjects_;
	std::list<EnemyBullet*> bullets_;
	float shotCoolTime = 2.0f; // 射撃のクールタイム

	// 当たったか
	bool isHit = false;
	float hitTimer_ = 0.0f;

	// HP
	int hp = 1;

};

