#include "TextRendererComponent.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "GameObject.h"
#include "RectTransformComponent.h"
#include "Sprite.h"
#include "TextureManager.h"

#pragma comment(lib, "Gdi32.lib")

namespace {
constexpr int kPadding = 2;

std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty()) {
		return L"";
	}
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
		static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) {
		return L"";
	}
	std::wstring wideText(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
		wideText.data(), length);
	return wideText;
}

std::string WideToUtf8(const std::wstring& text) {
	if (text.empty()) {
		return "";
	}
	const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		nullptr, 0, nullptr, nullptr);
	if (length <= 0) {
		return "";
	}
	std::string utf8(static_cast<size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length,
		nullptr, nullptr);
	return utf8;
}

int CALLBACK EnumerateFontFamily(const LOGFONTW* logFont, const TEXTMETRICW*, DWORD, LPARAM parameter) {
	auto* families = reinterpret_cast<std::vector<std::wstring>*>(parameter);
	families->emplace_back(logFont->lfFaceName);
	return 1;
}
}

void TextRendererComponent::Initialize() {
	textureKey_ = "__runtime_text_" + std::to_string(reinterpret_cast<uintptr_t>(this));
	RebuildTexture();
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(textureKey_);
	for (auto& outlineSprite : outlineSprites_) {
		outlineSprite = std::make_unique<Sprite>();
		outlineSprite->Initialize(textureKey_);
	}
}

void TextRendererComponent::Update() {
	if (dirty_) {
		RebuildTexture();
		if (sprite_) {
			sprite_->ChangeTexture(textureKey_);
		}
		for (auto& outlineSprite : outlineSprites_) {
			outlineSprite->ChangeTexture(textureKey_);
		}
	}
	if (!sprite_ || !GetGameObject()) {
		return;
	}

	Vector2 position{};
	Vector2 scale{};
	float rotation = 0.0f;
	if (const auto* rectTransform = GetGameObject()->GetComponent<RectTransformComponent>()) {
		position = rectTransform->position;
		rotation = rectTransform->rotation;
		scale = rectTransform->scale;
	} else {
		const Transform& transform = GetGameObject()->GetTransform()->transform;
		position = { transform.translate.x, transform.translate.y };
		rotation = transform.rotate.z;
		scale = { transform.scale.x, transform.scale.y };
	}

	sprite_->SetPosition(position);
	sprite_->SetRotation(rotation);
	sprite_->SetSize({ textSize_.x * scale.x, textSize_.y * scale.y });
	sprite_->SetColor(color_);
	sprite_->SetEmissive({ emissiveColor_.x * emissiveIntensity_, emissiveColor_.y * emissiveIntensity_,
		emissiveColor_.z * emissiveIntensity_ });
	sprite_->Update();

	if (outlineEnabled_) {
		const Vector2 outlineOffsets[8] = {
			{ -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 0.0f },
			{ 1.0f, 0.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },
		};
		const float cosine = std::cos(rotation);
		const float sine = std::sin(rotation);
		for (size_t index = 0; index < outlineSprites_.size(); ++index) {
			const Vector2 localOffset = {
				outlineOffsets[index].x * outlineThickness_ * scale.x,
				outlineOffsets[index].y * outlineThickness_ * scale.y,
			};
			const Vector2 rotatedOffset = {
				localOffset.x * cosine - localOffset.y * sine,
				localOffset.x * sine + localOffset.y * cosine,
			};
			auto& outlineSprite = outlineSprites_[index];
			outlineSprite->SetPosition({ position.x + rotatedOffset.x, position.y + rotatedOffset.y });
			outlineSprite->SetRotation(rotation);
			outlineSprite->SetSize({ textSize_.x * scale.x, textSize_.y * scale.y });
			outlineSprite->SetColor(outlineColor_);
			outlineSprite->SetEmissive({ 0.0f, 0.0f, 0.0f });
			outlineSprite->Update();
		}
	}
}

void TextRendererComponent::DrawTextSprite() {
	if (outlineEnabled_) {
		for (const auto& outlineSprite : outlineSprites_) {
			outlineSprite->Draw();
		}
	}
	if (sprite_) {
		sprite_->Draw();
	}
}

void TextRendererComponent::SetText(std::string text) {
	if (text_ != text) {
		text_ = std::move(text);
		dirty_ = true;
	}
}

void TextRendererComponent::SetFontFamily(std::wstring fontFamily) {
	if (!fontFamily.empty() && fontFamily_ != fontFamily) {
		fontFamily_ = std::move(fontFamily);
		dirty_ = true;
	}
}

void TextRendererComponent::SetFontFamilyUtf8(const std::string& fontFamily) {
	SetFontFamily(Utf8ToWide(fontFamily));
}

std::string TextRendererComponent::GetFontFamilyUtf8() const {
	return WideToUtf8(fontFamily_);
}

void TextRendererComponent::SetBold(bool bold) {
	if (bold_ != bold) {
		bold_ = bold;
		dirty_ = true;
	}
}

void TextRendererComponent::SetOutlineThickness(float thickness) {
	outlineThickness_ = (std::max)(0.0f, thickness);
}

void TextRendererComponent::SetEmissive(const Vector3& color, float intensity) {
	emissiveColor_ = color;
	emissiveIntensity_ = (std::max)(0.0f, intensity);
}

void TextRendererComponent::SetCharacterSpacing(float spacing) {
	if (characterSpacing_ != spacing) {
		characterSpacing_ = spacing;
		dirty_ = true;
	}
}

const std::vector<std::string>& TextRendererComponent::GetInstalledFontFamilies() {
	static const std::vector<std::string> families = [] {
		std::vector<std::wstring> wideFamilies;
		LOGFONTW logFont{};
		logFont.lfCharSet = DEFAULT_CHARSET;
		HDC hdc = GetDC(nullptr);
		if (hdc) {
			EnumFontFamiliesExW(hdc, &logFont, EnumerateFontFamily,
				reinterpret_cast<LPARAM>(&wideFamilies), 0);
			ReleaseDC(nullptr, hdc);
		}

		std::sort(wideFamilies.begin(), wideFamilies.end());
		wideFamilies.erase(std::unique(wideFamilies.begin(), wideFamilies.end()), wideFamilies.end());

		std::vector<std::string> utf8Families;
		utf8Families.reserve(wideFamilies.size());
		for (const auto& family : wideFamilies) {
			const std::string utf8Family = WideToUtf8(family);
			if (!utf8Family.empty()) {
				utf8Families.push_back(utf8Family);
			}
		}
		return utf8Families;
	}();
	return families;
}

void TextRendererComponent::SetFontSize(int fontSize) {
	const int clampedSize = (std::max)(1, fontSize);
	if (fontSize_ != clampedSize) {
		fontSize_ = clampedSize;
		dirty_ = true;
	}
}

void TextRendererComponent::SetMaxWidth(float maxWidth) {
	const float clampedWidth = (std::max)(0.0f, maxWidth);
	if (maxWidth_ != clampedWidth) {
		maxWidth_ = clampedWidth;
		dirty_ = true;
	}
}

void TextRendererComponent::RebuildTexture() {
	const std::wstring wideText = ConvertUtf8ToWide();
	HDC hdc = CreateCompatibleDC(nullptr);
	assert(hdc != nullptr);
	HFONT font = CreateFontW(-fontSize_, 0, 0, 0, bold_ ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, fontFamily_.c_str());
	assert(font != nullptr);
	const HGDIOBJ oldFont = SelectObject(hdc, font);
	const int characterSpacingPixels = static_cast<int>(std::round(characterSpacing_));
	SetTextCharacterExtra(hdc, characterSpacingPixels);

	RECT measured = { 0, 0, maxWidth_ > 0.0f ? static_cast<LONG>(maxWidth_) : 0, 0 };
	UINT drawFlags = DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT;
	if (maxWidth_ > 0.0f) {
		drawFlags |= DT_WORDBREAK;
	}
	DrawTextW(hdc, wideText.c_str(), static_cast<int>(wideText.size()), &measured, drawFlags);
	const int drawableCharacterCount = static_cast<int>(std::count_if(wideText.begin(), wideText.end(),
		[](wchar_t character) { return character != L'\r' && character != L'\n'; }));
	// DrawTextW does not include SetTextCharacterExtra in its measured rectangle on all Windows fonts.
	// Reserve the worst-case extra width so characters are never clipped at the texture edge.
	const int spacingWidth = (std::max)(0, characterSpacingPixels) * (std::max)(0, drawableCharacterCount - 1);
	const int contentWidth = (std::max)(1L, measured.right - measured.left) + spacingWidth;
	const int contentHeight = (std::max)(1L, measured.bottom - measured.top);
	const int width = contentWidth + kPadding * 2;
	const int height = contentHeight + kPadding * 2;

	BITMAPINFO bitmapInfo{};
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = -height; // Top-down DIB.
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	void* dibPixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
	assert(bitmap != nullptr && dibPixels != nullptr);
	const HGDIOBJ oldBitmap = SelectObject(hdc, bitmap);
	std::memset(dibPixels, 0, static_cast<size_t>(width) * height * 4);
	SetTextColor(hdc, RGB(255, 255, 255));
	SetBkMode(hdc, TRANSPARENT);
	RECT destination = { kPadding, kPadding, width - kPadding, height - kPadding };
	drawFlags &= ~DT_CALCRECT;
	DrawTextW(hdc, wideText.c_str(), static_cast<int>(wideText.size()), &destination, drawFlags);

	const auto* bgraPixels = static_cast<const uint8_t*>(dibPixels);
	std::vector<uint8_t> rgbaPixels(static_cast<size_t>(width) * height * 4);
	for (size_t index = 0; index < rgbaPixels.size(); index += 4) {
		const uint8_t coverage = (std::max)({ bgraPixels[index], bgraPixels[index + 1], bgraPixels[index + 2] });
		rgbaPixels[index] = 255;
		rgbaPixels[index + 1] = 255;
		rgbaPixels[index + 2] = 255;
		rgbaPixels[index + 3] = coverage;
	}

	SelectObject(hdc, oldBitmap);
	DeleteObject(bitmap);
	SelectObject(hdc, oldFont);
	DeleteObject(font);
	DeleteDC(hdc);

	TextureManager::GetInstance()->LoadTextureFromRGBA8(textureKey_, static_cast<uint32_t>(width),
		static_cast<uint32_t>(height), rgbaPixels);
	textSize_ = { static_cast<float>(width), static_cast<float>(height) };
	dirty_ = false;
}

std::wstring TextRendererComponent::ConvertUtf8ToWide() const {
	if (text_.empty()) {
		return L" ";
	}
	const std::wstring wideText = Utf8ToWide(text_);
	if (wideText.empty()) {
		return L"?";
	}
	return wideText;
}
