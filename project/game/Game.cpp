#include "Game.h"

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")

void Game::Initialize() {
	// 基底クラスの初期化
	M_Framework::Initialize();
	gameTimer_ = std::make_unique<GameTimer>();

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

	// ゲーム画面には描画しない専用RenderTextureへ、パーティクルを表示するImGuiエディタ。
	particlePreviewEditor_ = std::make_unique<ParticlePreviewEditor>();
	particlePreviewEditor_->Initialize(dxCommon, srvManager.get());

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
	// 時間の計測はゲーム全体で一度だけ行う。シーンはこの値を再計測しない。
	unscaledDeltaTime_ = gameTimer_->Tick();
	deltaTime_ = unscaledDeltaTime_ * SceneManager::GetInstance()->GetTimeScale();
	ParticleManager::GetInstance()->SetDeltaTime(deltaTime_);

	// シーンマネージャー更新
	SceneManager::GetInstance()->Update(deltaTime_, unscaledDeltaTime_);
	// エディタのUIとエミッタを先に更新し、その後のParticleManager::Updateで
	// 同じフレームのコンピュート更新へ反映する。
	particlePreviewEditor_->Update();
	// パーティクル更新
	ParticleManager::GetInstance()->Update();

	// トレイルエフェクト更新
	TrailEffectManager::GetInstance()->UpdateAll(deltaTime_);

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
	// 3D・雲・パーティクル・2Dテキストのすべてを描き終えた後にマスクを作る。
	// モデルは既存の深度を EQUAL で比較するため、隠れた面はマスクに現れない。
	PostEffect::GetInstance()->PreDrawObjectMask();
	// 空・雲は背景として先に書く。後から描くモデルと2Dテキストのマスクが
	// 前景として上書きされるため、テキストのOFF(値0)も確実に残る。
	if (RayMarching::GetInstance()->IsPostEffectEnabled()) {
		RayMarching::GetInstance()->DrawPostEffectMask();
	}
	SceneManager::GetInstance()->DrawPostEffectMask();
	PostEffect::GetInstance()->PostDrawObjectMask();

	// ポストエフェクト描画
	PostEffect::GetInstance()->PostDraw();
	PostEffect::GetInstance()->Draw();
	
	// 専用の3D RenderTextureへプレビュー用グループだけを描画する。
	particlePreviewEditor_->Render();

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
