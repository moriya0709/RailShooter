#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Camera.h"
#include "Sprite.h"
#include "Object.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "ImGuiFunction.h"
#include "BaseScene.h"
#include "PostEffect.h"
#include "RailCamera.h"
#include "Level.h"
#include "GameObject.h"
#include "GameTimer.h"
#include "Player.h"
#include "Calc.h"
#include "EnemySpawner.h"
#include "Enemy.h"
#include "CollisionManager.h"
#include "Line.h"

using namespace DirectX;

class SpriteCommon;
class ObjectCommon;

class GamePlayScene : public BaseScene {
public:
	// 初期化
	void Initialize() override;
	// 更新
	void Update() override;
	// 描画
	void Draw2D() override;
	void Draw3D() override;
	// 終了
	void Finalize() override;

private:
	

	Transform cameraTransform{
	   { 1.0f, 1.0f, 1.0f }, // scale
	   { 0.0f, 0.0f, 0.0f }, // rotate
	   { 0.0f, 0.0f, -5.0f } // translate
	};
	// パーティクル
	Transform transformParticle
	{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};


	// *ライティング* //

	// 平行光
	bool isDirectionalLight = false;
	Vector4 DirectionalLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector3 DirectionalLightDirection = { 0.0f, -1.0f, 0.0f };
	float DirectionalLightIntensity = 1.0f;
	// 環境光
	bool isAmbientLight = true;
	Vector4 AmbientLightColor = { 0.2f, 0.2f, 0.2f,1.0f };
	float AmbientLightIntensity = 1.0f;
	// ポイントライト
	bool isPointLight = false;
	Vector4 PointLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector3 PointLightPosition = { 1.0f, 1.0f, 0.0f };
	float PointLightIntensity = 1.0f;
	// スポットライト
	bool isSpotLight = false;
	Vector4 SpotLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector3 SpotLightPosition = { 0.0f, 0.0f, 0.0f };
	Vector3 SpotLightDirection = { 0.0f, 0.0f, 0.0f };
	float SpotLightRange = 10.0f;
	float SpotLightIntensity = 1.0f;

	// *ポストエフェクト* //

	// 反転
	bool isInversion = false;
	// グレースケール
	bool isGrayscale = false;


	// 放射線ブラー
	bool isRadialBlur = false;
	Vector2 blurCenter = { 0.5f,0.5f };
	float blurWidth = 0.01f;
	int blurSamples = 10;

	// ディスタンスフォグ
	bool isDistanceFog = false;
	Vector3 distanceFogColor = { 0.5f,0.5f,0.5f };
	float distanceStart = 5.0f;
	float distanceEnd = 20.0f;

	// ハイトフォグ
	bool isHeightFog = false;
	Vector3 heightFogColor = { 0.5f,0.5f,0.5f };
	float heightFogTop = 0.0f;
	float heightFogBottom = -5.0f;
	float heightFogDensity = 1.0f;

	// DOF
	bool isDOF = false;
	float focusDistance = 5.0f;
	float focusRange = 5.0f;
	float bokehRadius = 5.0f;

	// ブルーム
	float bloomThreshold = 1.0f;
	float bloomIntensity = 2.0f;
	float bloomBlurRadius = 1.0f;

	// レンズフレア
	bool isLensFlare = true;           // レンズフレアのON/OFF
	int lensFlareGhostCount = 6;   // ゴーストの数（例: 4～8）
	float lensFlareHaloWidth = 0.57f;      // ヘイロー（輪っか）の大きさ
	bool isACES = true;                 // ACESトーンマッピングをONにする
	float caIntensity = 0.05f;          // 色収差の強さ（最初は弱めに）

	// モーションブラー
	bool isMotionBlur = true;    // モーションブラーのON/OFF
	int motionBlurSamples = 16; // サンプル数（例：8〜16）
	float motionBlurScale = 1.0f;   // ブラーの強さ

	// レイマーチング
	//float rayMarchingTime = 0.0f; ;
	Vector3 rayMarchingSunDir = { 0.3f, -0.5f, 0.2f };
	float rayMarchingCloudCoverage = 0.00f;
	float rayMarchingCloudBottom = -90.0f;
	float rayMarchingCloudTop = 2900.0f;
	bool rayMarchingIsRialLight = false;
	bool rayMarchingIsAnimeLight = true;
	bool  rayMarchingIsMotionBlur = false;
	float  rayMarchingCloudOpacity = 0.04f;
	bool isStorm = false;
	float thunderFrequency = 0.3f;
	float thunderBrightness = 120.0f;
	float horizonHeight = 0.2f;
	float fogDensity = 0.0f;
	float fogHeight = 0.0f;
	float fogScattering = 0.5f;
	Vector3 fogColor = { 0.8f, 0.85f, 0.9f };

	// コントローラー
	int padX;
	int padY;

	// カメラモード
	bool isDebugCamera = true;

	// ギズモの操作モード
	ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
	// 現在選択されているオブジェクト（とりあえず0番目のオブジェクト用）
	GameObject* selectedObject = nullptr;
	// Game ウィンドウの現在位置。ImGui の移動・リサイズに合わせて毎フレーム更新する。
	Vector2 gameViewPosition = { 300.0f, 30.0f };
	Vector2 gameViewSize = { 1260.0f, 790.0f };
	bool isGameViewHovered = false;
	std::string dockDragWindow;
	DockGuideTarget dockGuideTarget = DockGuideTarget::None;
	bool isDockGuideVisible = false;
	std::string dockMergeTarget;
	std::vector<DockWindowInfo> dockWindowInfos;
	std::vector<std::string> mergedWindowTabs;
	std::string activeMergedWindow;

	// カメラ
	std::unique_ptr<Camera> camera = nullptr;
	std::unique_ptr<RailCamera> railCamera = nullptr;
	// 3Dオブジェクト
	std::unique_ptr <Object> object[2]{};
	// レベル
	std::unique_ptr <Level> level = nullptr;
	std::vector<std::unique_ptr <GameObject>> levelObjects;
	// スポナーのプレビュー用オブジェクト
	std::vector<std::unique_ptr<GameObject>> spawnerPreviewObjects;
	// スポーンする距離
	float spawnDistance = 20.0f;

	// 敵
	std::vector<std::unique_ptr<GameObject>> enemies;
	std::vector<std::unique_ptr<GameObject>> pathPreviewObjects; // パス移動の制御点のオブジェクト
	// プレイヤー
	std::unique_ptr<GameObject> defaultPlayerObject = nullptr;
	GameObject* playerObject = nullptr;
	Player* player = nullptr;
	// タイマー
	std::unique_ptr<GameTimer> gameTimer = nullptr;
	float deltaTime;

	// 当たり判定の線
	std::unique_ptr<Line> debugLineNormal;
	std::unique_ptr<Line> debugLineHit;

	std::shared_ptr<TrailEffect> trailEffect;
	Transform trailTransform = {
	{0.0f, 0.0f, 0.0f }, // translate
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 1.0f, 1.0f, 1.0f }  // scale
	};
	float width = 0.1f; // 弾の幅
	float trailMaxLifeTime = 1.0f; // トレイルの寿命

	// レベルデータからオブジェクト生成、配置
	void CreateLevel();
	// Gizmo
	void GizmoUpdate(bool showEditorControls);

	// 当たり判定
	void CollisionUpdate();

};
