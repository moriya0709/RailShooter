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

protected:
	bool isDead_ = false; // 消滅フラグ

};

