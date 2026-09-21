#include "Game.h"

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")

void Game::Initialize() {
	// 基底クラスの初期化
	M_Framework::Initialize();

#pragma region 基盤システム

	// カメラマネージャ
	CameraManager::GetInstance();

	// スプライト共通部の初期化
	SpriteCommon::GetInstance()->Initialize(dxCommon);

	// 3dスプライト共通部の初期化
	ObjectCommon::GetInstance()->Initialize(dxCommon);

	// 線共通部の初期化
	LineCommon::GetInstance()->Initialize(dxCommon);

	// SRVマネージャ
	srvManager = std::make_unique<SrvManager>();
	srvManager->Initialize(dxCommon);

	// ImGui
	imGuiManager = std::make_unique<ImGuiManager>();
	imGuiManager->Initialize(windowAPI.get(), dxCommon, srvManager.get());

	// テクスチャマネージャの初期化
	TextureManager::GetInstance()->Initialize(dxCommon, srvManager.get());
	// 3Dモデルマネージャの初期化
	ModelManager::GetInstance()->Initialize(dxCommon, srvManager.get());
	// Particleマネージャ
	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager.get(), "Resource/plane", "plane.obj");

#pragma endregion

#pragma region 最初のシーン
	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("Resource/trail/trail.png");

	// パーティクルマネージャ初期化
	ParticleManager::GetInstance()->CreateParticleGroup("obj", "Resource/plane", "plane.obj", "Resource/particle/particle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("ring", ParticleManager::GetInstance()->Ring(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("cylinder", ParticleManager::GetInstance()->Cylinder(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("cone", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("sphere", ParticleManager::GetInstance()->Sphere(), "Resource/particle/gradationLine.png");
	// トルネード
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado1", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado2", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado3", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado4", ParticleManager::GetInstance()->Ring(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado5", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado6", ParticleManager::GetInstance()->Cone(), "Resource/particle/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado7", "Resource/plane", "plane.obj", "Resource/particle/particle.png",0);
	ParticleManager::GetInstance()->CreateParticleGroup("Tornado8", "Resource/plane", "plane.obj", "Resource/particle/particle.png",0);

	// Resource 以下のモデルの場所だけを登録する。
	// 実体のロードは、シーンやエディタがモデルを使用した時点で行う。
	ModelManager::GetInstance()->RegisterModelsInResourceDirectory();
	
	// 追加のアニメーションを読み込む
	ModelManager::GetInstance()->LoadAnimation("walk.gltf","walk", "Resource/human", "walk.gltf");
	ModelManager::GetInstance()->LoadAnimation("walk.gltf","sneakWalk", "Resource/human", "sneakWalk.gltf");

	// ライティング
	LightManager::GetInstance()->Initialize();

	// サウンド
	SoundManager::GetInstance()->Initialize();
	SoundManager::GetInstance()->Load("bgm", "game.mp3");

	// ポストエフェクト
	PostEffect::GetInstance()->Initialize(dxCommon, windowAPI.get(),srvManager.get());
	
	// レイマーチング
	RayMarching::GetInstance()->Initialize(srvManager.get(), windowAPI.get());

	// トレイルエフェクト
	TrailEffectManager::GetInstance()->Initialize();

	// シーンマネージャーの生成
	// 最初のシーン生成
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(move(sceneFactory_));
	// シーンマネージャーに最初のシーンをセット
	SceneManager::GetInstance()->ChangeScene("TITLE");

	Skybox::GetInstance()->Initialize(dxCommon, "Resource/rostock_laage_airport_4k.dds");
	
#pragma endregion
}

void Game::Update() {
	// ImGui受付開始
	imGuiManager->Begin();

	// 　基底クラス
	M_Framework::Update();

	// シーンマネージャー更新
	SceneManager::GetInstance()->Update();

	// パーティクル更新
	ParticleManager::GetInstance()->Update();

	// トレイルエフェクト更新
	TrailEffectManager::GetInstance()->UpdateAll();

	// ImGui受付終了
	imGuiManager->End();
}

void Game::Draw() {
	// 描画前処理
	M_Framework::BeginFrame();
	srvManager->PreDraw();
	PostEffect::GetInstance()->PreDraw();

	// シーンマネージャー描画(3D)
	SceneManager::GetInstance()->Draw3D();

	PostEffect::GetInstance()->TransitionDepthBuffer(
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// ★ここで毎フレーム雲を計算する
	uint32_t depthSrvIndex = PostEffect::GetInstance()->GetDepthSrvIndex();
	RayMarching::GetInstance()->ComputeCloud(depthSrvIndex);
	// 計算結果を画面に合成
	RayMarching::GetInstance()->Draw();
	// 深度バッファを「書き込み/テスト用」に戻す
	PostEffect::GetInstance()->TransitionDepthBuffer(D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// パーティクル描画
	ParticleManager::GetInstance()->Draw();
	// トレイルエフェクト描画
	TrailEffectManager::GetInstance()->RenderAll();

	// 2DをHDRシーンバッファへ描画する。これによりエミッシブ文字もブルーム対象になる。
	PostEffect::GetInstance()->SetSceneColorRenderTarget();
	SceneManager::GetInstance()->Draw2D();

	// ポストエフェクト描画
	PostEffect::GetInstance()->PostDraw();
	PostEffect::GetInstance()->Draw();
	

	// ImGui描画
	imGuiManager->Draw();

	// 描画後処理
	M_Framework::EndFrame();
}

void Game::Finalize() {
	// ImGuiの終了処理
	imGuiManager->Finalize();

	// 　サウンドマネージャー終了
	SoundManager::GetInstance()->Finalize();

	// 基底クラスの終了処理
	M_Framework::Finalize();
}
