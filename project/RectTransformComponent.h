#pragma once
#include "Component.h"
#include "CommonStructs.h"

// SpriteRendererComponent が参照する画面座標系の変換。
// 3D Transform と併用でき、UI の配置だけを独立して指定できる。
class RectTransformComponent : public Component {
public:
	Vector2 position = { 960.0f, 540.0f };
	float rotation = 0.0f;
	Vector2 scale = { 1.0f, 1.0f };
};
