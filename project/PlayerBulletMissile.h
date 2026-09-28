#pragma once
#include <memory>

#include "PlayerBullet.h"
#include "TrailEffect.h"
#include "Calc.h"
#include "ParticleEmitter.h"

class Enemy;

enum LaunchDirection {
	Left,   // 左方向
	Right,  // 右方向
	Up,     // 上方向
	Down    // 下方向
};

class PlayerBulletMissile : public PlayerBullet {
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

	// 左右の発射位置（初期角度）を設定する関数
	void SetInitAngle(bool isRight);
	// 螺旋の回転方向を設定する（true: 時計回り / false: 反時計回り）
	void SetSpiralDirection(bool isRight) {
		spiralDirection = isRight ? 1.0f : -1.0f;
	}

private:
	Transform transform = {
	{0.0f, 0.0f, 0.0f }, // translate
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 1.0f, 1.0f, 1.0f }  // scale
	};
	Vector3 previousTranslate_ = { 0.0f, 0.0f, 0.0f };

	LaunchDirection launchDirection = LaunchDirection::Right; // 発射方向

	float width = 0.05f; // 弾の幅
	float trailMaxLifeTime = 1.0f; // トレイルの寿命
	Vector3 velocity = { 0.0f, 0.0f, 30.0f }; // 弾の速度
	Vector3 projectileVelocity_ = { 0.0f, 0.0f, 0.5f };
	Vector3 inheritedVelocity_ = { 0.0f, 0.0f, 0.0f };
	float speed = 2.0f; // 弾の移動速度
	float deathTimer = 0.0f; // 消滅までの時間
	static constexpr float kLifeTime = 5.0f; // 弾の寿命（秒）
	Enemy* target_ = nullptr;	// ロックオン対象の敵
	int damage = 1; // 弾のダメージ量

	// ホーミングを開始する敵との距離
	float homingPower = 2.0f; // ホーミングの強さ

	// ★ 螺旋（トルネード）移動用パラメータ
	float homingDistance = 30.0f; // 急旋回を開始する距離
	Vector3 centerPos = { 0.0f, 0.0f, 0.0f }; // 螺旋の中心軸座標
	float spiralAngle = 0.0f;                 // 現在の回転角度
	float spiralSpeed = 5.0f;                // 周囲を回る速度
	float spiralRadius = 0.5f;                // 螺旋（竜巻）の太さ（半径）
	float spiralDirection = 1.0f;				// 回転方向用フラグ（1.0f: 時計回り, -1.0f: 反時計回り）

	std::unique_ptr <ParticleEmitter> particle = nullptr;
};

