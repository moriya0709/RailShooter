#pragma once
#include "Component.h"
#include "CommonStructs.h"
#include "Calc.h"

// SpriteRendererComponent が参照する画面座標系の変換。
// 3D Transform と併用でき、UI の配置だけを独立して指定できる。
class RectTransformComponent : public Component {
public:

	Vector2 GetPosition() const {return position_;}
	float GetRotation() const {return rotation_;}
	Vector2 GetScale() const {return scale_;}

	void SetPosition(Vector2 position) { position_ = position; }
	void SetRotation(float rotation) { rotation_ = rotation; }
	void SetScale(Vector2 scale) { scale_ = scale; }

private:
	Vector2 position_ = { 960.0f, 540.0f };
	float rotation_ = 0.0f;
	Vector2 scale_ = { 1.0f, 1.0f };
};
