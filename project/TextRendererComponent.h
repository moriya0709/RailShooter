#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Component.h"
#include "CommonStructs.h"

class Sprite;

// A UTF-8 UI text component. It rasterizes an installed Windows font at runtime.
class TextRendererComponent : public Component {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override {}
	// Call from a 2D draw pass after SpriteCommon has been configured.
	void DrawTextSprite();

	void SetText(std::string text);
	const std::string& GetText() const { return text_; }
	void SetFontFamily(std::wstring fontFamily);
	const std::wstring& GetFontFamily() const { return fontFamily_; }
	void SetFontFamilyUtf8(const std::string& fontFamily);
	std::string GetFontFamilyUtf8() const;
	static const std::vector<std::string>& GetInstalledFontFamilies();
	void SetBold(bool bold);
	bool IsBold() const { return bold_; }
	void SetOutlineEnabled(bool enabled) { outlineEnabled_ = enabled; }
	bool IsOutlineEnabled() const { return outlineEnabled_; }
	void SetOutlineThickness(float thickness);
	float GetOutlineThickness() const { return outlineThickness_; }
	void SetOutlineColor(const Vector4& color) { outlineColor_ = color; }
	const Vector4& GetOutlineColor() const { return outlineColor_; }
	void SetEmissive(const Vector3& color, float intensity);
	const Vector3& GetEmissiveColor() const { return emissiveColor_; }
	float GetEmissiveIntensity() const { return emissiveIntensity_; }
	void SetCharacterSpacing(float spacing);
	float GetCharacterSpacing() const { return characterSpacing_; }
	void SetFontSize(int fontSize);
	int GetFontSize() const { return fontSize_; }
	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }
	// Zero disables wrapping; a positive value enables word wrapping at this width.
	void SetMaxWidth(float maxWidth);
	float GetMaxWidth() const { return maxWidth_; }
	Vector2 GetTextSize() const { return textSize_; }

private:
	void RebuildTexture();
	std::wstring ConvertUtf8ToWide() const;

	std::unique_ptr<Sprite> sprite_;
	std::array<std::unique_ptr<Sprite>, 8> outlineSprites_;
	std::string text_;
	std::wstring fontFamily_ = L"Meiryo UI";
	std::string textureKey_;
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 outlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 emissiveColor_ = { 1.0f, 1.0f, 1.0f };
	Vector2 textSize_ = { 0.0f, 0.0f };
	int fontSize_ = 32;
	float maxWidth_ = 0.0f;
	float outlineThickness_ = 1.0f;
	float emissiveIntensity_ = 0.0f;
	float characterSpacing_ = 0.0f;
	bool bold_ = false;
	bool outlineEnabled_ = false;
	bool dirty_ = true;
};
