#pragma once
#include <memory>

#include "Enemy.h"
#include "Calc.h"
#include "Object.h"

class EnemyNormal : public Enemy {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;

	void SetTransform(const Transform& transform) override { this->transform = transform; }

	Vector3 GetTranslate() const override { return transform.translate; }
	void OnCollision() override { isDead_ = true; }
	bool IsDead() const override { return isDead_; }

public:
	Transform transform{
		{ 1.0f, 1.0f, 1.0f }, // scale
		{ 0.0f, 0.0f, 0.0f }, // rotate
		{ 0.0f, 0.0f, 0.0f }  // translate
	};

	bool isDead_ = false;

	// 3Dオブジェクト
	std::unique_ptr <Object> object = nullptr;

};

