#pragma once
#include <cstdint>
#include <string>
#include <cassert>
#include <random>

#include "Calc.h"
#include "Blend.h"
#include "CommonStructs.h"

class ParticleEmitter {
public:
	// 初期化
	void Initialize(std::string name, const Transform& transform, uint32_t count, float frequency);
	// 更新
	void Update();

	// パーティクル発生
	void Emit();
	// アクティブ設定
	void SetActive(const std::string& name);

	// セーブ
	void SaveParticle(const std::string& filePath);
	// ロード
	void LoadParticle(const std::string& filePath);

	// エディター
	void Editor();

	// setter
	void SetTranslate(Vector3 translate) { emitter->translate = translate; }

private:
	// パーティクルエミッタの設定
	std::unique_ptr<Emitter> emitter;
	// パーティクル共通データ
	std::uniform_real_distribution<float> distPosition; // ランダムな座標範囲
	std::uniform_real_distribution<float> distScale; // ランダムなスケール範囲
	std::uniform_real_distribution<float> distRotate; // ランダムな回転範囲
	std::uniform_real_distribution<float> distVelocity; // ランダムな速度範囲
	std::uniform_real_distribution<float> distTime; // ランダムな寿命範囲
	bool isRandPosition[3] = { true }; // ランダムな座標にするかどうか
	bool isRandScale[3] = { true }; // ランダムなスケールにするかどうか
	bool isRandRotate[3] = { true }; // ランダムな回転にするかどうか
	bool isRandVelocity[3] = { true }; // ランダムに動かすかどうか
	Vector4 color; // 色
	Vector4 finalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float colorChangeSpeed = 1.0f;
	float lifeTime; // 寿命
	bool isColorChange[4] = { false }; // 色変更するかどうか
	bool isScaleChange[3] = { false }; // スケール変更するかどうか
	float scaleAdd = 0.005f; // スケール変更量
	Vector2 uvScale = { 1.0f, 1.0f }; // UVスケール
	Vector2 uvScrollSpeed = { 0.1f, 0.0f }; // UVスクロール速度
	Vector2 uvOffset = { 0.0f, 0.0f }; // UVオフセット
	int32_t useNoise = 0;
	Vector3 burnColor = { 1.0f, 0.25f, 0.0f }; // ふちの色

	char fileName[20]; //パーティクルのファイル名
	std::string name; //!< エミッタ名


	// デルタタイム(60fps固定)
	const float kDeltaTime = 1.0f / 60.0f;

	// エミッシブ
	float emissive = 10.0f;
	// ブレンドモード
	BlendMode blendMode;

};