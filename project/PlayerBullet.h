#pragma once
#include "TrailEffect.h"
#include "Calc.h"
#include "CollisionManager.h"
#include "Component.h"

class Enemy;

class PlayerBullet : public Component {
public:
	virtual ~PlayerBullet() = default;

	// inheritedVelocity は発射元の1フレームあたりのワールド移動量。
	virtual void Initialize(Vector3 position, Enemy* target, const Vector3& inheritedVelocity) = 0;
	virtual void Update() override = 0;

	// 消滅しているか
	bool IsDead() const { return isDead_; }

	// ダメージ量を取得
	virtual int GetDamage() const = 0;
	// 削除される敵のポインタを受け取り、もし自分のターゲットなら破棄する
	virtual void RemoveTarget(const Enemy* enemy) = 0;

	virtual Vector3 GetTranslate() const = 0;
	// レール移動を含む発射元の現在速度を毎フレーム反映する。
	virtual void SetInheritedVelocity(const Vector3& inheritedVelocity) = 0;
	// 高速移動の連続衝突判定に使う、更新前の位置。
	virtual Vector3 GetPreviousTranslate() const { return GetTranslate(); }
	virtual void OnCollision() { isDead_ = true; }

	// OBBを取得
	virtual OBB GetOBB() = 0;

protected:
	bool isDead_ = false; // 消滅フラグ

};

