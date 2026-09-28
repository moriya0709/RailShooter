#pragma once
#include <memory>

#include "PlayerBullet.h"
#include "TrailEffect.h"
#include "Calc.h"

class Enemy;

class PlayerBulletNormal : public PlayerBullet {
public:
	void Initialize(const Vector3 position, Enemy* target, const Vector3& inheritedVelocity) override;
	void Update() override;

	Vector3 GetTranslate() const override { return transform.translate; }
	void SetInheritedVelocity(const Vector3& inheritedVelocity) override { inheritedVelocity_ = inheritedVelocity; }
	Vector3 GetPreviousTranslate() const override { return previousTranslate_; }

	// OBBを取得
	OBB GetOBB() override;

	// ダメージ量を取得
	int GetDamage() const override { return damage; }
	// 削除される敵のポインタを受け取り、もし自分のターゲットなら破棄する
	void RemoveTarget(const Enemy* enemy) override;

private:
	Transform transform = {
	{0.0f, 0.0f, 0.0f }, // translate
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 1.0f, 1.0f, 1.0f }  // scale
	};
	Vector3 previousTranslate_ = { 0.0f, 0.0f, 0.0f };
	float width = 0.05f; // 弾の幅
	float trailMaxLifeTime = 0.3f; // トレイルの寿命
	Vector3 velocity = { 0.0f, 0.0f, 30.0f }; // 弾の速度
	Vector3 projectileVelocity_ = { 0.0f, 0.0f, 0.5f };
	Vector3 inheritedVelocity_ = { 0.0f, 0.0f, 0.0f };
	float speed = 2.0f; // 弾の移動速度
	float deathTimer = 0.0f; // 消滅までの時間
	static constexpr float kLifeTime = 5.0f; // 弾の寿命（秒）
	Enemy* target_ = nullptr;	// ロックオン対象の敵
	int damage = 1; // 弾のダメージ量
	float homingPower = 0.5f; // 誘導の強さ（0.01f ～ 0.1f 程度がおすすめ。高すぎると往復します）

	std::shared_ptr<TrailEffect> trailEffect;
};

