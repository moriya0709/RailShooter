#pragma once
#include <string>
#include <vector>
#include <memory>
#include <list>

#include "Calc.h"
#include "CollisionManager.h"
#include "EnemyBullet.h"

class Enemy {
public:
	virtual ~Enemy() = default;

	virtual void Initialize() = 0;
	virtual void Update(Vector3 playerPosition, float currentPlayerProgress) = 0;
	virtual void Draw() = 0;

	// 指定した制御点リスト上の座標を算出する関数
	virtual Vector3 GetSplinePosition(const std::vector<Vector3>& points, float progress) = 0;

	virtual void SetTransform(const Transform& transform) = 0;

	virtual Vector3 GetTranslate() const = 0;
	virtual void OnCollisionBullet(int damage) = 0;
	virtual bool IsDead() const = 0;
	virtual bool IsHit() const = 0;

	// 移動パターンの設定
	virtual void SetMovePattern(const std::string& pattern) = 0;
	// 制御点の設定
	virtual void SetControlPoints(const std::vector<Vector3>& points) = 0;
	// レール追従用の制御点の設定
	virtual void SetRailPoints(const std::vector<Vector3>& points) = 0;
	// レール追従用のオフセットの設定
	virtual void SetRailOffsetProgress(float offset) = 0;

	virtual const std::list<std::unique_ptr<EnemyBullet>>& GetBullets() = 0;

	// OBBを取得
	virtual OBB GetOBB() = 0;

};