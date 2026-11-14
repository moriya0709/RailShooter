#pragma once
#include <memory>
#include <string>
#include "Component.h"
#include "CommonStructs.h"

class Sprite;

class SpriteRendererComponent : public Component {
public:
	// Sprite の GPU リソースを生成する。texturePath_ は生成後も差し替え可能。
	void Initialize() override;
	// RectTransform を優先し、無い場合は 3D Transform の XY / Z 回転を UI 値として使う。
	void Update() override;
	void Draw() override {}
	// 2D 描画パスから呼び出す実描画処理。通常の Draw() とは分離している。
	void DrawSprite();

	void SetTexture(const std::string& texturePath);
	const std::string& GetTexturePath() const { return texturePath_; }

	void SetSize(const Vector2& size) { size_ = size; }
	Vector2 GetSize() const { return size_; }
	void SetEmissive(const Vector3& color, float intensity);
	const Vector3& GetEmissiveColor() const { return emissiveColor_; }
	float GetEmissiveIntensity() const { return emissiveIntensity_; }
	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }
	void SetUVScale(const Vector2& scale) { uvScale_ = scale; }
	const Vector2& GetUVScale() const { return uvScale_; }
	void SetUVOffset(const Vector2& offset) { uvOffset_ = offset; }
	const Vector2& GetUVOffset() const { return uvOffset_; }
	void SetUVRotation(float rotation) { uvRotation_ = rotation; }
	float GetUVRotation() const { return uvRotation_; }

private:
	// 初期化後にのみ有効。GameObject の寿命とともに Sprite も破棄される。
	std::unique_ptr<Sprite> sprite_;
	std::string texturePath_ = "Resource/title/title.png";
	Vector2 size_ = { 128.0f, 128.0f };
	Vector3 emissiveColor_ = { 1.0f, 1.0f, 1.0f };
	float emissiveIntensity_ = 0.0f;
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector2 uvScale_ = { 1.0f, 1.0f };
	Vector2 uvOffset_ = { 0.0f, 0.0f };
	float uvRotation_ = 0.0f;
};
