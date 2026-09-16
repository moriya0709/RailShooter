#include "SpriteRendererComponent.h"
#include "GameObject.h"
#include "RectTransformComponent.h"
#include "Sprite.h"

void SpriteRendererComponent::Initialize() {
	// テクスチャパスを保持してから生成するため、初期化前の SetTexture にも対応する。
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(texturePath_);
}

void SpriteRendererComponent::Update() {
	if (!sprite_ || !GetGameObject()) {
		return;
	}

	if (const auto* rectTransform = GetGameObject()->GetComponent<RectTransformComponent>()) {
		// UI 用の変換があるオブジェクトは、ワールド座標ではなく画面座標で描画する。
		sprite_->SetPosition(rectTransform->position);
		sprite_->SetRotation(rectTransform->rotation);
		sprite_->SetSize({ size_.x * rectTransform->scale.x, size_.y * rectTransform->scale.y });
	} else {
		// RectTransform を持たない既存オブジェクトとの互換用フォールバック。
		const Transform& transform = GetGameObject()->GetTransform()->transform;
		sprite_->SetPosition({ transform.translate.x, transform.translate.y });
		sprite_->SetRotation(transform.rotate.z);
		sprite_->SetSize({ size_.x * transform.scale.x, size_.y * transform.scale.y });
	}
	sprite_->Update();
}

void SpriteRendererComponent::DrawSprite() {
	if (sprite_) {
		sprite_->Draw();
	}
}

void SpriteRendererComponent::SetTexture(const std::string& texturePath) {
	texturePath_ = texturePath;
	if (sprite_) {
		// 生成済みの場合だけ GPU 側の SRV を即座に更新する。
		sprite_->ChangeTexture(texturePath_);
	}
}
