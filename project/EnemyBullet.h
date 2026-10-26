#pragma once
#include "Calc.h"
#include "CollisionManager.h"
#include "Component.h"

class EnemyBullet : public Component {
public:
	virtual void Initialize(Vector3 position, Vector3 playerPosition) = 0;
	virtual void Update() override = 0;
	void SetDeltaTime(float deltaTime) { deltaTime_ = (std::max)(0.0f, deltaTime); }

	// 消滅しているか
	bool IsDead() const { return isDead_; }

	// ダメージ量を取得
	virtual int GetDamage() const = 0;
	// 衝突処理
	virtual void OnCollision() { isDead_ = true; }
	// OBBを取得
	virtual OBB GetOBB() = 0;

protected:
	bool isDead_ = false; // 消滅フラグ
	float deltaTime_ = 1.0f / 60.0f;
};

