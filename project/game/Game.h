#pragma once
#include <Windows.h>
#include <dxgidebug.h>
#include <strsafe.h>
#include <minidumpapiset.h>
#include "SpriteCommon.h"
#include "ObjectCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "ImGuiFunction.h"
#include "M_Framework.h"
#include "GamePlayScene.h"
#include "SoundManager.h"
#include "ModelManager.h"
#include "PostEffect.h"
#include "RayMarching.h"
#include "TrailEffectManager.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "Skybox.h"
#include "LineCommon.h"
#include "LightManager.h"
#include "ParticlePreviewEditor.h"
#include "GameTimer.h"

class Game : public M_Framework {
public:
	// 初期化
	void Initialize() override;
	// 更新
	void Update() override;
	// 描画
	void Draw() override;
	// 終了
	void Finalize() override;

private:

	// SRVマネージャ
	std::unique_ptr<SrvManager> srvManager = nullptr;
	// ImGuiマネージャ
	std::unique_ptr <ImGuiManager> imGuiManager = nullptr;
	// シーンファクトリー
	std::unique_ptr <SceneFactory> sceneFactory_ = nullptr;
	// スカイボックス
	std::unique_ptr <Skybox> skybox_ = nullptr;
	// ImGui内で確認するための、ゲーム本編から独立したパーティクルプレビュー。
	std::unique_ptr<ParticlePreviewEditor> particlePreviewEditor_ = nullptr;
	// 唯一のゲーム時計。各シーンはここで計測済みの時間だけを受け取る。
	std::unique_ptr<GameTimer> gameTimer_ = nullptr;
	float deltaTime_ = 0.0f;
	float unscaledDeltaTime_ = 0.0f;

};
