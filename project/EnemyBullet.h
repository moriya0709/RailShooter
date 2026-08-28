#pragma once
#include "Calc.h"
#include "CollisionManager.h"

class EnemyBullet {
public:
	virtual void Initialize(Vector3 position, Vector3 playerPosition) = 0;
	virtual void Update() = 0;

	// 消滅しているか
	bool IsDead() const { return isDead_; }

	// 衝突処理
	virtual void OnCollision() { isDead_ = true; }
	// OBBを取得
	virtual OBB GetOBB() = 0;

protected:
	bool isDead_ = false; // 消滅フラグ
};

