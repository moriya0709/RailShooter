#pragma once
#include <memory>

#include "PlayerBullet.h"
#include "TrailEffect.h"
#include "Calc.h"

class Enemy;

class PlayerBulletNormal : public PlayerBullet {
public:
	void Initialize(const Vector3 position, Enemy* target) override;
	void Update() override;

	Vector3 GetTranslate() const override { return transform.translate; }

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
	float width = 0.05f; // 弾の幅
	float trailMaxLifeTime = 1.0f; // トレイルの寿命
	Vector3 velocity = { 0.0f, 0.0f, 30.0f }; // 弾の速度
	float speed = 0.5f; // 弾の移動速度
	float deathTimer = 0.0f; // 消滅までの時間
	static constexpr float kLifeTime = 5.0f; // 弾の寿命（秒）
	Enemy* target_ = nullptr;	// ロックオン対象の敵
	int damage = 1; // 弾のダメージ量

	std::shared_ptr<TrailEffect> trailEffect;
};

