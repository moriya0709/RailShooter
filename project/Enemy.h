#pragma once
#include "Calc.h"

class Enemy {
public:
	virtual ~Enemy() = default;

	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

	virtual void SetTransform(const Transform& transform) = 0;

	virtual Vector3 GetTranslate() const = 0;
	virtual void OnCollision() = 0;
	virtual bool IsDead() const = 0;
};