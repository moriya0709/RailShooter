#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>
#include "CameraComponent.h"
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
	void Update(float deltaTime, float unscaledDeltaTime) override;
	// 描画
	void Draw2D() override;
	void Draw3D() override;
	void DrawPostEffectMask() override;
	// 終了
	void Finalize() override;

private:
	

	Transform cameraTransform{
	   { 1.0f, 1.0f, 1.0f }, // scale
		{ 0.005f, -6.395f, 0.0f }, // rotate
		{ 1998.308f, 113.211f, -91.868f  } // translate
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
	bool isDirectionalLight = true;
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
	// 太陽のライティング
	bool isSunLight = true;

	// *ポストエフェクト* //

	// 反転
	bool isInversion = false;
	// グレースケール
	bool isGrayscale = false;


	// 放射線ブラー
	bool isRadialBlur = false;
	Vector2 blurCenter = { 0.5f,0.5f };
	float blurWidth = 0.0f;
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
	float bloomThreshold = 2.0f;
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

	// 色収差
	bool isFullScreenCA = false; // 画面全体の色収差ON/OFF
	float fullScreenCAIntensity = 0.02f; // 画面全体の色収差の強さ
	// ビネット
	bool isVignette = false; // ビネットのON/OFF
	float vignetteIntensity = 0.5f; // ビネットの強さ
	Vector3 vignetteColor = { 1.0f, 0.0f, 0.0f };   // 色

	// スピードディストーション
	bool isSpeedDistortion = false; // スピードディストーションのON/OFF
	float speedDistortionStrength = 1.0f; // 歪みの強さ

	// 集中線
	bool isConcentrationLines = false; // ON/OFF
	float concentrationLineIntensity = 1.0f; // 線の濃さ
	Vector2 concentrationLineCenter = { 0.5f, 0.5f }; // 中心座標 (通常 0.5, 0.5)
	float concentrationLineDensity = 10.0f; // 線の密度（本数）
	float concentrationLineLength = 0.5f; // 線の長さ（中心からの開始距離 0.0〜1.0）
	float concentrationLineSpeed = 1.0f; // アニメーション速度

	// ピンチエフェクト
	bool isPinch = false; // ON/OFF
	float pinchStrength = 0.5f; // 歪みの強さ（正の値で吸い込み、負の値で膨張）
	Vector2 pinchCenter = { 0.5f, 0.5f }; // 歪みの中心 (通常 0.5, 0.5)
	float pinchRadius = 0.5f; // 歪みが影響する半径

	// モノクロ
	bool isTwoColor = false; // 二値化
	float threshold = 0.5f; // 白と黒の境界値 (0.0~1.0)
	float contrast = 1.0f; // コントラストの強さ
	float grayscaleProgress = 0.0f; // 白黒化の進行度 (0.0=カラー, 1.0=完全な白黒)

	// アウトライン
	bool isOutline = false;  // ON/OFF
	float outlineThreshold = 0.5f; // 境界値
	Vector4 outlineColor = { 1.0f, 1.0f, 1.0f, 1.0f };    // 色

	// 平滑化
	bool isBoxFilter = false;   // ON/OFF

	// ディゾルブ
	bool isDissolve = false;     // ON/OFF
	float dissolveThreshold = 0.5f;    // 境界値
	float dissolveEdgeWidth = 0.1f;    // エッジの幅
	float dissolveNoiseScale = 1.0f;   // ノイズの細かさ

	Vector3 dissolveNoiseColor = { 0.5f, 0.5f, 0.5f }; // ノイズの色
	Vector3 dissolveEdgeColor = { 0.0f, 0.0f, 0.0f };   // エッジの色

	// ランダムノイズ
	bool isRandomNoise = false;  // ON/OFF

	// 目を閉じるエフェクト (アイブリンク)
	bool isEyeBlink = false; // ON/OFF
	float eyeBlinkProgress = 1.5f; // 閉じる進行度 (0.0 = 全開, 1.0 = 完全に閉じる)

	// モニターオフ (ブラウン管)
	bool isMonitorOff = false; // ON/OFF
	float monitorOffProgress = 0.0f; // 進行度 (0.0 = 通常, 1.0 = 完全に消える)
	Vector2 monitorOffCenter = { 0.5f, 0.5f }; // 消える最終地点 (X, Y 共に 0.0 ～ 1.0。中央は 0.5, 0.5)

	// グリッチノイズ
	bool isGlitch = false; // ON/OFF
	float glitchIntensity = 1.0f; // 歪みと色ズレの強さ (0.0 ～ 1.0)

	float intensity = 0.0f; // エフェクトの強さ

	// レイマーチング
	//float rayMarchingTime = 0.0f; ;
	Vector3 rayMarchingSunDir = { -0.13f, -0.16f, -1.0f };
	float rayMarchingCloudCoverage = 0.16f;
	float rayMarchingCloudBottom = 5000.0f;
	float rayMarchingCloudTop = 4000.0f;
	bool rayMarchingIsRialLight = false;
	bool rayMarchingIsAnimeLight = true;
	bool  rayMarchingIsMotionBlur = false;
	float  rayMarchingCloudOpacity = 0.002f;
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
	// 直前フレームのレール再生状態。停止遷移の検出に使う。
	bool wasRailCameraActive = false;
	uint64_t lastRailStartRevision_ = 0;

	// ギズモの操作モード
	#ifdef USE_IMGUI
	ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
	int selectedTextMeshVertex_ = -1;
	#endif
	// 現在選択されているオブジェクト（とりあえず0番目のオブジェクト用）
	GameObject* selectedObject = nullptr;
	// Game ウィンドウの現在位置。ImGui の移動・リサイズに合わせて毎フレーム更新する。
	Vector2 gameViewPosition = { 300.0f, 30.0f };
	Vector2 gameViewSize = { 1260.0f, 790.0f };
	bool isGameViewHovered = false;

	// カメラ
	// The normal camera belongs to an EMPTY in levelObjects. The fallback is
	// only used when loading older levels that do not contain a Camera component.
	GameObject* cameraObject = nullptr;
	std::unique_ptr<GameObject> fallbackCameraObject = nullptr;
	CameraComponent* camera = nullptr;
	std::unique_ptr<RailCamera> railCamera = nullptr;
	// 空文字列は従来どおり未グループの RailPoint をレールとして使う。
	std::string activeRailGroupName_;
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

	// スタート演出
	bool isStartAction = true;
	// 開始レールの再生が実際に開始された後だけ、終了判定を有効にする。
	bool StartRailFirst_ = false;
	bool isFadeIn = false;
	bool isFadeOut = false;
	float fadeTime = 120.0f;

	// デス演出
	bool isDeathAction = false;
	float deathParticleElapsed_ = 0.0f;
	float deathSlowTime_ = 1.0f;
	float alpha = 0.0f;
	float textAlphaTime = 0.8f;
	float effectTime = 0.0f;


	// レベルデータからオブジェクト生成、配置
	void CreateLevel();
	void ResetEnemySpawning();
	// レールカメラのプリセットを作成する
	void CreateRailPresetFromGroup(const std::string& groupName, const std::string& presetName);
	// プリセットの接続・生成ロジック
	void AppendRailPreset(const RailPresetData& preset);
	// プリセットの順番をレールカメラグループに反映する
	void RebuildRailGroupFromSequence(const std::string& groupName);

	// Gizmo
	void GizmoUpdate(bool showEditorControls);


};
