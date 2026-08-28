#pragma once
#include <memory>

#include "EnemyBullet.h"
#include "Calc.h"
#include "TrailEffect.h"

class EnemyNormalBullet : public EnemyBullet {
public:
	void Initialize(Vector3 position,Vector3 playerPosition) override;
	void Update() override;



	// OBBを取得
	OBB GetOBB() override;

private:
	Transform transform{
		{ 1.0f, 1.0f, 1.0f }, // scale
		{ 0.0f, 0.0f, 0.0f }, // rotate
		{ 0.0f, 0.0f, 0.0f }  // translate
	};

	float width = 1.0f; // 弾の幅
	float trailMaxLifeTime = 1.0f; // トレイルの寿命
	Vector3 velocity = { 0.0f, 0.0f, 10.0f }; // 弾の速度
	float speed = 0.5f; // 弾の移動速度
	float deathTimer = 0.0f; // 消滅までの時間
	static constexpr float kLifeTime = 5.0f; // 弾の寿命（秒）

	std::shared_ptr<TrailEffect> trailEffect;
};

