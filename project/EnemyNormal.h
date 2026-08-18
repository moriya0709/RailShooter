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

public:
	Transform transform{
		{ 1.0f, 1.0f, 1.0f }, // scale
		{ 0.0f, 0.0f, 0.0f }, // rotate
		{ 0.0f, 0.0f, 0.0f }  // translate
	};

	// 3Dオブジェクト
	std::unique_ptr <Object> object = nullptr;

};

