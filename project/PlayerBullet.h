#pragma once
#include "TrailEffect.h"
#include "Calc.h"

class PlayerBullet {
public:
	virtual ~PlayerBullet() = default;

	virtual void Initialize(Vector3 position) = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

	// 消滅しているか
	bool IsDead() const { return isDead_; }

	virtual Vector3 GetTranslate() const = 0;
	virtual void OnCollision() { isDead_ = true; }

protected:
	bool isDead_ = false; // 消滅フラグ

};

