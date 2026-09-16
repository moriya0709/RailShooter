#pragma once
#include "TrailEffect.h"
#include "Calc.h"
#include "CollisionManager.h"
#include "Component.h"

class Enemy;

class PlayerBullet : public Component {
public:
	virtual ~PlayerBullet() = default;

	virtual void Initialize(Vector3 position, Enemy* target) = 0;
	virtual void Update() override = 0;

	// 消滅しているか
	bool IsDead() const { return isDead_; }

	// ダメージ量を取得
	virtual int GetDamage() const = 0;
	// 削除される敵のポインタを受け取り、もし自分のターゲットなら破棄する
	virtual void RemoveTarget(const Enemy* enemy) = 0;

	virtual Vector3 GetTranslate() const = 0;
	virtual void OnCollision() { isDead_ = true; }

	// OBBを取得
	virtual OBB GetOBB() = 0;

protected:
	bool isDead_ = false; // 消滅フラグ

};

