#include "TitleScene.h"
#include "ObjectCommon.h"
#include "SpriteCommon.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "TransformComponent.h"
#include "ModelRendererComponent.h"
#include "AnimatorComponent.h"
#include "RailPointComponent.h"
#include "EnemySpawnerComponent.h"
#include "EnemyNormal.h"
#include "SpriteRendererComponent.h"
#include "TextRendererComponent.h"
#include "RectTransformComponent.h"
#include "ColliderComponent.h"
#include "LevelEditorCommon.h"
#include "SkyBox.h"
#include "LineCommon.h"
#include "ModelManager.h"
#include <cstring>
#include <cctype>
#include <filesystem>

void TitleScene::Initialize() {

	// カメラ初期化
	camera = std::make_unique<Camera>();
	camera->SetRotate({ cameraTransform.rotate });
	camera->SetTranslate({ cameraTransform.translate });

	railCamera = std::make_unique<RailCamera>();
	railCamera->Initialize();


	// カメラマネージャ登録
	CameraManager::GetInstance()->AddCamera("main", camera.get());
	CameraManager::GetInstance()->SetActiveCamera("main");

	// レベル
	level = std::make_unique<Level>();
	level->LoadJson("TitleScene");
	CreateLevel();


	// 3Dオブジェクト
	for (int i = 0; i < 2; i++) {
		object[i] = std::make_unique <Object>();
		object[i]->Initialize(camera.get());
	}

	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object[0]->SetModel("cube.gltf");
	object[1]->SetModel("cube.gltf");

	// 音声再生
	//SoundManager::GetInstance()->Play("bgm");

	// タイマー
	gameTimer = std::make_unique<GameTimer>();

	// 当たり判定の線
	debugLineNormal = std::make_unique<Line>();
	debugLineNormal->Initialize(camera.get());
	debugLineNormal->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f }); // 緑色を固定セット

	debugLineHit = std::make_unique<Line>();
	debugLineHit->Initialize(camera.get());
	debugLineHit->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // 赤色を固定セット

	// トレイルエフェクト
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", trailTransform, width, trailMaxLifeTime);

}

void TitleScene::Update() {
	// 入力取得
	auto input = Input::GetInstance();
	// カメラ更新
	CameraManager::GetInstance()->Update();
	// 時間更新
	deltaTime = gameTimer->Tick();

	trailEffect->Editor();

	// 1. レールカメラの更新（レール上の現在位置・回転を計算）
	// 2. ★ GameObject の座標を RailCamera の制御点として毎フレーム上書き（同期）する
	if (railCamera) {
		railCamera->points.clear(); // 一旦リセット

		for (const auto& levelObject : levelObjects) {
			if (auto* railPoint = levelObject->GetComponent<RailPointComponent>(); railPoint && railPoint->IsEnabled() && levelObject->IsActive()) {
				auto* transformComp = levelObject->GetTransform();
				RailPoint p{};
				p.position = transformComp->transform.translate;
				p.rotate = transformComp->transform.rotate;
				railCamera->points.push_back(p);
			}
		}
	}

	// 3. その後、RailCamera自身の更新処理を呼ぶ
	if (railCamera) {
		railCamera->EditorUpdate();
		railCamera->Update();
	}

	// タイトルではレールカメラをそのまま表示カメラの基準にする。
	const Vector3 railCameraPosition = railCamera->GetBasePosition();
	const Vector3 railCameraRotation = railCamera->GetBaseRotation();


	// デバックカメラ処理
	if (isDebugCamera) {
		const Vector2 mousePosition = input->GetMouseScreen();
		const bool isMouseInGameView = ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Game") &&
			mousePosition.x >= gameViewPosition.x && mousePosition.x < gameViewPosition.x + gameViewSize.x &&
			mousePosition.y >= gameViewPosition.y && mousePosition.y < gameViewPosition.y + gameViewSize.y;
		// ImGui の捕捉状態ではなく、Game ビューの中にカーソルがあるかで判定する。
		if (isMouseInGameView) {
			camera->DebugCameraUpdate();
		}
	} else {
		camera->SetTranslate(railCameraPosition);
		camera->SetRotate(railCameraRotation);
		camera->Update();
	}

	// *スポナーの距離判定と敵の更新* //

	// エディタカメラ操作中ではない（ゲームプレイ中）場合のみ起動・生成を進める
	if (!isDebugCamera) {
		// タイトルでは表示カメラの位置をスポナーの起動基準にする。
		Vector3 targetPos = camera->GetTranslate();

		for (auto& levelObject : levelObjects) {
			auto* spawner = levelObject->GetComponent<EnemySpawnerComponent>();
			if (!spawner || !spawner->IsEnabled() || !levelObject->IsActive()) continue;
			// 一定距離に入ったら起動し、時間経過で敵が生成されて返ってくる
			auto spawnedEnemies = spawner->Spawn(deltaTime, targetPos);

			// 返ってきた敵をシーンの敵リストに移動
			for (auto& newEnemy : spawnedEnemies) {
				enemies.push_back(std::move(newEnemy));
			}
		}
	}

	// カメラの現在の進行度を取得
	float cameraProgress = railCamera->GetRailT();

	// シーンに存在するすべての敵の更新処理
	for (auto& enemyObject : enemies) {
		if (auto* enemy = enemyObject->GetComponent<Enemy>()) {
			enemy->SetUpdateContext(camera->GetTranslate(), cameraProgress);
			enemyObject->Update();
		}
	}

	// レベルオブジェクト
	for (auto& object : levelObjects) {
		if (auto* enemy = object->GetComponent<Enemy>()) {
			enemy->SetUpdateContext(camera->GetTranslate(), cameraProgress);
		}
		object->Update();
	}

	// ENTERキーを押したら
	//if (input->TriggerKey(DIK_RETURN)) {
	//	// ゲームプレイシーン(次シーン)を生成
	//	SceneManager::GetInstance()->ChangeScene("TITLE");
	//}

	// 数字の０キーが押されていたら
	if (input->TriggerKey(DIK_0)) {
		OutputDebugStringA("Hit 0\n"); // 出力ウィンドウに「Hit ０」と表示

		// エフェクト有効化(色反転)
		PostEffect::GetInstance()->SetInversion(true);
	}

	// * 3Dオブジェクト* //
	for (int i = 0; i < 2; i++) {
		object[i]->Update();
	}

	// --- 撃破された敵の削除 ---
	for (auto it = enemies.begin(); it != enemies.end();) {
		auto* enemy = (*it)->GetComponent<Enemy>();
		if (enemy && enemy->IsDead()) {
			it = enemies.erase(it); // 実際の削除処理
		} else {
			++it;
		}
	}
	for (auto& levelObject : levelObjects) {
		if (auto* enemy = levelObject->GetComponent<Enemy>(); enemy && enemy->IsDead()) {
			levelObject->SetActive(false);
		}
	}

	// *当たり判定の線* //
	// 毎フレーム描画前に前フレームの線データをクリア
	debugLineNormal->Clear();
	debugLineHit->Clear();
#ifdef _DEBUG
	// 敵キャラクターの当たり判定も一括登録可能
	for (const auto& enemyObject : enemies) {
		auto* enemy = enemyObject->GetComponent<Enemy>();
		if (!enemy) continue;
		if (enemy->IsHit()) {
			DrawOBB(debugLineHit.get(), LevelEditorCommon::GetColliderOBB(enemyObject.get(), enemy->GetOBB())); // 赤用のバッファに追加
		} else {
			DrawOBB(debugLineNormal.get(), LevelEditorCommon::GetColliderOBB(enemyObject.get(), enemy->GetOBB())); // 緑用のバッファに追加
		}
	}

	// Empty に追加した Collider も緑のOBBラインで表示する。
	for (const auto& levelObject : levelObjects) {
		const auto* collider = levelObject->GetComponent<ColliderComponent>();
		if (!collider || !collider->IsEnabled() || !levelObject->IsActive()) {
			continue;
		}
		DrawOBB(debugLineNormal.get(), collider->GetOBB());
	}

	for (const auto& enemyObject : enemies) {
		auto* enemy = enemyObject->GetComponent<Enemy>();
		if (!enemy) continue;
		for (const auto& bullet : enemy->GetBullets()) {
			DrawOBB(debugLineNormal.get(), bullet->GetOBB());
		}
	}

#endif


#pragma region ライティング
	// *ライティング* //
	auto lightManager = LightManager::GetInstance();
	// 平行光
	lightManager->SetDirectionalLightActive(isDirectionalLight);
	lightManager->SetDirectionalLightDirection(DirectionalLightDirection);
	lightManager->SetDirectionalLightColor(DirectionalLightColor);
	lightManager->SetDirectionalLightIntensity(DirectionalLightIntensity);
	// 環境光
	lightManager->SetAmbientLightActive(isAmbientLight);
	lightManager->SetAmbientLightColor(AmbientLightColor);
	lightManager->SetAmbientLightIntensity(AmbientLightIntensity);
	// ポイントライト
	lightManager->SetPointLightActive(isPointLight);
	lightManager->SetPointLightColor(PointLightColor);
	lightManager->SetPointLightPosition(PointLightPosition);
	lightManager->SetPointLightIntensity(PointLightIntensity);
	// スポットライト
	lightManager->SetSpotLightActive(isSpotLight);
	lightManager->SetSpotLightColor(SpotLightColor);
	lightManager->SetSpotLightPosition(SpotLightPosition);
	lightManager->SetSpotLightDirection(SpotLightDirection);
	lightManager->SetSpotLightRange(SpotLightRange);
	lightManager->SetSpotLightIntensity(SpotLightIntensity);

	lightManager->Update();
#pragma endregion

#pragma region ポストエフェクト
	// *ポストエフェクト* //
	PostEffect::GetInstance()->Update(camera.get());

	// 反転
	PostEffect::GetInstance()->SetInversion(isInversion);
	// グレースケール
	PostEffect::GetInstance()->SetGrayscale(isGrayscale);
	// 放射線ブラー
	PostEffect::GetInstance()->SetRadialBlur(isRadialBlur);
	PostEffect::GetInstance()->SetBlurCenter(blurCenter);
	PostEffect::GetInstance()->SetBlurWidth(blurWidth);
	PostEffect::GetInstance()->SetBlurSamples(blurSamples);
	// ディスタンスフォグ
	PostEffect::GetInstance()->SetDistanceFog(isDistanceFog);
	PostEffect::GetInstance()->SetDistanceFogColor(distanceFogColor);
	PostEffect::GetInstance()->SetDistanceFogStart(distanceStart);
	PostEffect::GetInstance()->SetDistanceFogEnd(distanceEnd);
	// ハイトフォグ
	PostEffect::GetInstance()->SetHeightFog(isHeightFog);
	PostEffect::GetInstance()->SetHeightFogColor(heightFogColor);
	PostEffect::GetInstance()->SetHeightFogTop(heightFogTop);
	PostEffect::GetInstance()->SetHeightFogBottom(heightFogBottom);
	PostEffect::GetInstance()->SetHeightFogDensity(heightFogDensity);
	PostEffect::GetInstance()->HightFogUpdate(camera.get());
	// DOF
	PostEffect::GetInstance()->SetDOF(isDOF);
	PostEffect::GetInstance()->SetFocusDistance(focusDistance);
	PostEffect::GetInstance()->SetBokehRadius(bokehRadius);
	PostEffect::GetInstance()->SetFocusRange(focusRange);
	// ブルーム
	PostEffect::GetInstance()->SetBloomIntensity(bloomIntensity);
	PostEffect::GetInstance()->SetBloomThreshold(bloomThreshold);
	PostEffect::GetInstance()->SetBloomBlurRadius(bloomBlurRadius);
	// レンズフレア
	PostEffect::GetInstance()->SetLensFlare(isLensFlare);
	PostEffect::GetInstance()->SetLensFlareGhostCount(lensFlareGhostCount);
	PostEffect::GetInstance()->SetLensFlareHaloWidth(lensFlareHaloWidth);
	PostEffect::GetInstance()->SetIsACES(isACES);
	PostEffect::GetInstance()->SetCAIntensity(caIntensity);
	// モーションブラー
	PostEffect::GetInstance()->SetMotionBlur(isMotionBlur);
	PostEffect::GetInstance()->SetMotionBlurSamples(motionBlurSamples);
	PostEffect::GetInstance()->SetMotionBlurScale(motionBlurScale);

#pragma endregion

#pragma region レイマーチング
	// レイマーチング
	RayMarching::GetInstance()->Update(camera.get());
	//rayMarching->SetTime(rayMarchingTime);
	RayMarching::GetInstance()->SetSunDir(rayMarchingSunDir);
	RayMarching::GetInstance()->SetCloudCoverage(rayMarchingCloudCoverage);
	RayMarching::GetInstance()->SetCloudTop(rayMarchingCloudBottom);
	RayMarching::GetInstance()->SetCloudBottom(rayMarchingCloudTop);
	RayMarching::GetInstance()->SetRialLight(rayMarchingIsRialLight);
	RayMarching::GetInstance()->SetAnimeLight(rayMarchingIsAnimeLight);
	RayMarching::GetInstance()->SetMotionBlur(rayMarchingIsMotionBlur);
	RayMarching::GetInstance()->SetCloudOpacity(rayMarchingCloudOpacity);
	RayMarching::GetInstance()->SetStorm(isStorm);
	RayMarching::GetInstance()->SetThunderFrequency(thunderFrequency);
	RayMarching::GetInstance()->SetThunderBrightness(thunderBrightness);
	RayMarching::GetInstance()->SetHorizonHeight(horizonHeight);
	RayMarching::GetInstance()->SetFogDensity(fogDensity);
	RayMarching::GetInstance()->SetFogHeight(fogHeight);
	RayMarching::GetInstance()->SetFogScattering(fogScattering);
	RayMarching::GetInstance()->SetFogColor(fogColor);

#pragma endregion

#ifdef USE_IMGUI
	// 最初だけ Unity 風に配置し、以後は通常の ImGui ウィンドウとして移動・リサイズできる。
	// ImGui が imgui.ini に位置とサイズを保存するため、Visual Studio のツールウィンドウのように
	// ユーザーが決めた配置が次回起動時にも再現される。
	ImGuiIO& editorIO = ImGui::GetIO();
	const float editorTopBarHeight = 30.0f;
	const float editorLeftPaneWidth = 300.0f;
	const float editorRightPaneWidth = 360.0f;
	const float editorBottomPaneHeight = 260.0f;
	const float hierarchyHeight = (std::max)(180.0f, (editorIO.DisplaySize.y - editorTopBarHeight) * 0.42f);
	const ImVec2 initialGameViewPosition(editorLeftPaneWidth, editorTopBarHeight);
	const ImVec2 initialGameViewSize(
		(std::max)(100.0f, editorIO.DisplaySize.x - editorLeftPaneWidth - editorRightPaneWidth),
		(std::max)(100.0f, editorIO.DisplaySize.y - editorTopBarHeight - editorBottomPaneHeight));

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(editorIO.DisplaySize.x, editorTopBarHeight), ImGuiCond_Always);
	ImGui::Begin("Editor Toolbar", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
	static char levelFileName[128] = "TitleScene";
	LevelEditorCommon::DrawToolbar(*level, levelObjects, levelFileName, IM_ARRAYSIZE(levelFileName));
	ImGui::End();

	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Game")) {
		ImGui::SetNextWindowPos(initialGameViewPosition, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(initialGameViewSize, ImGuiCond_FirstUseEver);
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGuiFunction::GetInstance()->TrackDockableWindow("Game");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Game");
		const ImVec2 gameContentMin = ImGui::GetCursorScreenPos();
		const ImVec2 gameWindowPosition = ImGui::GetWindowPos();
		const ImVec2 contentRegionMax = ImGui::GetWindowContentRegionMax();
		const ImVec2 gameContentMax(gameWindowPosition.x + contentRegionMax.x, gameWindowPosition.y + contentRegionMax.y);
		gameViewPosition = { gameContentMin.x, gameContentMin.y };
		gameViewSize = {
			(std::max)(1.0f, gameContentMax.x - gameContentMin.x),
			(std::max)(1.0f, gameContentMax.y - gameContentMin.y)
		};
		// 最終合成を Game のコンテンツ領域へ限定する。これにより ImGui の下にはゲームを描画しない。
		PostEffect::GetInstance()->SetOutputViewport(gameViewPosition.x, gameViewPosition.y, gameViewSize.x, gameViewSize.y);
		isGameViewHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		ImGui::GetWindowDrawList()->AddRect(
			gameContentMin,
			gameContentMax,
			IM_COL32(115, 160, 210, 210));
		ImGui::End();
	} else {
		// Game タブが非選択なら映像も非表示にする。
		PostEffect::GetInstance()->SetOutputViewport(0.0f, 0.0f, 1.0f, 1.0f);
		isGameViewHovered = false;
	}
	const ImVec2 gameViewWindowPosition(gameViewPosition.x, gameViewPosition.y);
	const ImVec2 gameViewWindowSize(gameViewSize.x, gameViewSize.y);

	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Settings")) {
		ImGui::SetNextWindowPos(ImVec2(editorLeftPaneWidth, editorIO.DisplaySize.y - editorBottomPaneHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(gameViewWindowSize.x, editorBottomPaneHeight), ImGuiCond_FirstUseEver);
		ImGui::Begin("Settings");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Settings");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Settings");

		// フレームレートの取得と表示
		float fps = ImGui::GetIO().Framerate;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", fps > 0.0f ? 1000.0f / fps : 0.0f, fps);

		ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.1f, -500.0f, 500.0f);
		ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f, -10.0f, 10.0f);

		ImGui::Checkbox("isDebugCamera", &isDebugCamera);

	#pragma region ライティング
		// *ライティング* //
		ImGui::Text("Lighting"); // ライティングのテキスト

		// 平行光
		if (ImGui::TreeNode("DirectionalLight")) {
			ImGui::Checkbox("OnOff", &isDirectionalLight);
			if (isDirectionalLight) {
				ImGui::ColorEdit4("Color", &DirectionalLightColor.x);
				ImGui::DragFloat3("Direction", &DirectionalLightDirection.x, 0.01f, -100.0f, 100.0f);
				ImGui::DragFloat("Intensity", &DirectionalLightIntensity, 0.01f, 0.0f, 10.0f);
			}
			ImGui::TreePop();
		}
		// 環境光
		if (ImGui::TreeNode("AmbientLight")) {
			ImGui::Checkbox("OnOff", &isAmbientLight);
			if (isAmbientLight) {
				ImGui::ColorEdit4("Color", &AmbientLightColor.x);
				ImGui::DragFloat("Intensity", &AmbientLightIntensity, 0.01f, 0.0f, 10.0f);
			}

			ImGui::TreePop();
		}
		// ポイントライト
		if (ImGui::TreeNode("PointLight")) {
			ImGui::Checkbox("OnOff", &isPointLight);
			if (isPointLight) {
				ImGui::ColorEdit4("Color", &PointLightColor.x);
				ImGui::DragFloat3("Position", &PointLightPosition.x, 0.01f, -100.0f, 100.0f);
				ImGui::DragFloat("Intensity", &PointLightIntensity, 0.01f, 0.0f, 10.0f);
			}

			ImGui::TreePop();
		}
		// スポットライト
		if (ImGui::TreeNode("SpotLight")) {
			ImGui::Checkbox("OnOff", &isSpotLight);
			if (isSpotLight) {
				ImGui::ColorEdit4("Color", &SpotLightColor.x);
				ImGui::DragFloat3("Position", &SpotLightPosition.x, 0.01f, -100.0f, 100.0f);
				ImGui::DragFloat3("Direction", &SpotLightDirection.x, 0.01f, -100.0f, 100.0f);
				ImGui::DragFloat("Range", &SpotLightRange, 0.01f, 0.0f, 100.0f);
				ImGui::DragFloat("Intensity", &SpotLightIntensity, 0.01f, 0.0f, 10.0f);
			}

			ImGui::TreePop();
		}

	#pragma endregion

	#pragma region ポストエフェクト
		// *ポストエフェクト* //
		ImGui::Text("PostEffect"); // ポストエフェクトのテキスト

		// 反転
		if (ImGui::TreeNode("inversion")) {
			ImGui::Checkbox("OnOff", &isInversion);

			ImGui::TreePop();
		}
		// グレースケール
		if (ImGui::TreeNode("grayscale")) {
			ImGui::Checkbox("OnOff", &isGrayscale);

			ImGui::TreePop();
		}
		// 放射線ブラー
		if (ImGui::TreeNode("radialBlur")) {
			ImGui::Checkbox("OnOff", &isRadialBlur);

			if (isRadialBlur) {
				ImGui::DragFloat2("blurCenter", &blurCenter.x, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("blurWidth", &blurWidth, 0.001f, 0.0f, 0.1f);
				ImGui::DragInt("blurSamples", &blurSamples, 1, 1, 100);
			}

			ImGui::TreePop();
		}
		// ディスタンスフォグ
		if (ImGui::TreeNode("distanceFog")) {
			ImGui::Checkbox("OnOff", &isDistanceFog);

			if (isDistanceFog) {
				ImGui::ColorEdit3("fogColor", &distanceFogColor.x);
				ImGui::DragFloat("fogStart", &distanceStart, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat("fogEnd", &distanceEnd, 0.1f, 0.0f, 100.0f);
			}

			ImGui::TreePop();
		}
		// ハイトフォグ
		if (ImGui::TreeNode("heightFog")) {
			ImGui::Checkbox("OnOff", &isHeightFog);

			if (isHeightFog) {
				ImGui::ColorEdit3("heightFogColor", &heightFogColor.x);
				ImGui::DragFloat("heightFogTop", &heightFogTop, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat("heightFogBottom", &heightFogBottom, 0.1f, -100.0f, 100.0f);
				ImGui::DragFloat("heightFogDensity", &heightFogDensity, 0.01f, 0.0f, 10.0f);
			}

			ImGui::TreePop();
		}
		// DOF
		if (ImGui::TreeNode("DOF")) {
			ImGui::Checkbox("OnOff", &isDOF);

			if (isDOF) {
				ImGui::DragFloat("focusDistance", &focusDistance, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat("bokehRadius", &bokehRadius, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat("focusRange", &focusRange, 0.1f, 0.0f, 100.0f);
			}

			ImGui::TreePop();
		}
		// ブルーム
		if (ImGui::TreeNode("Bloom")) {
			ImGui::DragFloat("bloomThreshold", &bloomThreshold, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("bloomIntensity", &bloomIntensity, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("bloomRadius", &bloomBlurRadius, 0.01f, 0.0f, 10.0f);

			ImGui::TreePop();
		}
		// レンズフレア
		if (ImGui::TreeNode("LensFlare")) {
			ImGui::Checkbox("OnOff", &isLensFlare);

			if (isLensFlare) {
				ImGui::DragInt("lensFlareGhostCount", &lensFlareGhostCount, 1, 0, 10);
				ImGui::DragFloat("lensFlareHaloWidth", &lensFlareHaloWidth, 0.01f, 0.0f, 10.0f);
				ImGui::Checkbox("isACES", &isACES);
				ImGui::DragFloat("caIntensity", &caIntensity, 0.001f, 0.0f, 10.0f);
			}
			ImGui::Text("%.3f", PostEffect::GetInstance()->GetLensFlareGhostDispersal());

			ImGui::TreePop();
		}
		// モーションブラー
		if (ImGui::TreeNode("MotionBlur")) {
			ImGui::Checkbox("OnOff", &isMotionBlur);

			if (isLensFlare) {
				ImGui::DragInt("motionBlurSamples", &motionBlurSamples, 1, 0, 20);
				ImGui::DragFloat("motionBlurScale", &motionBlurScale, 0.01f, 0.0f, 10.0f);
			}

			ImGui::TreePop();
		}

	#pragma endregion

	#pragma region レイマーチング
		// レイマーチング
		//ImGui::DragFloat("rayMarchingTime", &rayMarchingTime, 0.1f,0.0f,10.0f);
		ImGui::DragFloat3("rayMarchingSunDir", &rayMarchingSunDir.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("rayMarchingCloudCoverage", &rayMarchingCloudCoverage, 0.01f, -5.0f, 10.0f);
		ImGui::DragFloat("rayMarchingCloudBottom", &rayMarchingCloudBottom, 10.0f, -5000.0f, 5000.0f);
		ImGui::DragFloat("rayMarchingCloudTop", &rayMarchingCloudTop, 10.0f, -5000.0f, 5000.0f);
		ImGui::Checkbox("rayMarchingIsRialLight", &rayMarchingIsRialLight);
		ImGui::Checkbox("rayMarchingIsAnimeLight", &rayMarchingIsAnimeLight);
		ImGui::Checkbox("rayMarchingIsMotionBlur", &rayMarchingIsMotionBlur);
		ImGui::DragFloat("rayMarchingCloudOpacity", &rayMarchingCloudOpacity, 0.001f, 0.0f, 0.1f);
		ImGui::Checkbox("isStorm", &isStorm);
		ImGui::DragFloat("thunderFrequency", &thunderFrequency, 0.001f, 0.0f, 10.0f);
		ImGui::DragFloat("thunderBrightness", &thunderBrightness, 0.01f, 0.0f, 300.0f);
		ImGui::DragFloat("horizonHeight", &horizonHeight, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("fogDensity", &fogDensity, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("fogHeight", &fogHeight, 1.0f, 0.0f, 100.0f);
		ImGui::DragFloat("fogScattering", &fogScattering, 0.01f, 0.0f, 1.0f);
		ImGui::ColorEdit3("fogColor", &fogColor.x);

	#pragma endregion

		// Gizmo
		GizmoUpdate(true);
		ImGui::End();
	}

	// --- Unity 風 Hierarchy ウィンドウ ---
	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Hierarchy")) {
		ImGui::SetNextWindowPos(ImVec2(0.0f, editorTopBarHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(editorLeftPaneWidth, hierarchyHeight), ImGuiCond_FirstUseEver);
		ImGui::Begin("Hierarchy");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Hierarchy");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Hierarchy");
		if (ImGui::Button("Create Empty")) {
			const std::string objectName = "Empty_" + std::to_string(levelObjects.size() + 1);
			auto emptyObject = std::make_unique<GameObject>(objectName);
			emptyObject->Initialize();

			selectedObject = emptyObject.get();
			levelObjects.push_back(std::move(emptyObject));

			ObjectData emptyData{};
			emptyData.type = "EMPTY";
			emptyData.name = objectName;
			emptyData.transform = selectedObject->GetTransform()->transform;
			level->GetLevelData()->objects.push_back(emptyData);
		}

		ImGui::Separator();
		for (size_t index = 0; index < levelObjects.size(); ++index) {
			auto* object = levelObjects[index].get();
			const bool isEmpty = !object->GetComponent<ModelRendererComponent>() &&
				!object->GetComponent<SpriteRendererComponent>() &&
				!object->GetComponent<TextRendererComponent>() &&
				!object->GetComponent<RailPointComponent>() &&
				!object->GetComponent<EnemySpawnerComponent>() &&
				!object->GetComponent<Enemy>();
			const char* icon = isEmpty ? "[E]" : "[O]";
			const std::string label = std::string(icon) + " " + object->GetName() + "##Hierarchy" + std::to_string(index);
			if (ImGui::Selectable(label.c_str(), selectedObject == object)) {
				selectedObject = object;
			}
		}
		ImGui::End();
	}

	// --- アセットブラウザ ウィンドウ ---
	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Asset Browser")) {
		ImGui::SetNextWindowPos(ImVec2(0.0f, editorTopBarHeight + hierarchyHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(editorLeftPaneWidth, (std::max)(100.0f, editorIO.DisplaySize.y - editorTopBarHeight - hierarchyHeight)), ImGuiCond_FirstUseEver);
		ImGui::Begin("Asset Browser");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Asset Browser");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Asset Browser");

		// アセットとして追加したいモデルのファイルリスト（本来はフォルダ内を自動全検索してもOK）
		std::vector<std::string> modelFiles = ModelManager::GetInstance()->GetLoadedModelNames();
		static std::vector<std::string> textureFiles;
		static bool textureFilesLoaded = false;
		if (!textureFilesLoaded) {
			textureFilesLoaded = true;
			try {
				for (const auto& entry : std::filesystem::recursive_directory_iterator("Resource")) {
					if (!entry.is_regular_file()) continue;
					std::string extension = entry.path().extension().string();
					std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds") {
						textureFiles.push_back(entry.path().generic_string());
					}
				}
			} catch (const std::filesystem::filesystem_error&) {
				OutputDebugStringA("[Asset Browser] Resource directory could not be enumerated.\n");
			}
		}

		ImGui::Text("Models (drag to the viewport or a Model field):");
		ImGui::Separator();

		for (const auto& fileName : modelFiles) {
			// リスト項目（ツリーやselectable、ボタンなど）を表示
			ImGui::Selectable(fileName.c_str());

			// ★ 1. ドラッグのソース（送信側）の設定
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				// ペイロード（渡すデータ）の識別IDキー文字列を設定（例: "DND_MODEL_FILE"）
				// 渡すデータとして「ファイル名文字列」を指定
				ImGui::SetDragDropPayload("DND_MODEL_FILE", fileName.c_str(), fileName.size() + 1);

				// ドラッグ中にカーソル横に表示されるツールチップ
				ImGui::Text("Spawn: %s", fileName.c_str());

				ImGui::EndDragDropSource();
			}
		}

		ImGui::Separator();
		ImGui::Text("Textures (drag to a Sprite Texture field):");
		for (const auto& texturePath : textureFiles) {
			ImGui::Selectable(texturePath.c_str());
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("DND_TEXTURE_FILE", texturePath.c_str(), texturePath.size() + 1);
				ImGui::Text("Texture: %s", texturePath.c_str());
				ImGui::EndDragDropSource();
			}
		}

		// レールの制御点
		if (ImGui::Button("Add Rail Point")) {
			// 1. 新しい GameObject を生成
			const std::string objectName = "RailPoint_" + std::to_string(levelObjects.size() + 1);
			auto newObject = std::make_unique<GameObject>(objectName);

			// 2. TransformComponent の追加と配置設定
			auto transformComp = newObject->AddComponent<TransformComponent>();

			transformComp->transform.translate = camera->GetTranslate();
			transformComp->transform.rotate = camera->GetRotate();
			transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

			// 3. ModelRendererComponent の追加（モデルは固定で "rail.obj" を指定）
			auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel("rail.obj");
			newObject->AddComponent<RailPointComponent>();

			// 4. 初期化
			newObject->Initialize();

			// 5. 生成したオブジェクトを即座に選択状態にする（ギズモですぐ動かせるように）
			selectedObject = newObject.get();

			// 6. ゲームシーンの更新リストに追加
			levelObjects.push_back(std::move(newObject));

			// 7. JSON保存用の LevelData にも「RAIL」タイプとして登録
			ObjectData newObjectData;
			newObjectData.type = "RAIL"; // ★ ここを RAIL にする
			newObjectData.name = objectName;
			newObjectData.file_name = "rail.obj";
			newObjectData.transform = transformComp->transform;
			newObjectData.components = { "ModelRenderer", "RailPoint" };

			level->GetLevelData()->objects.push_back(newObjectData);

			OutputDebugStringA("★★★ Added New Rail Point!\n");
		}
		// 敵のスポーンイベント地点
		if (ImGui::Button("Add Enemy Spawner")) {
			const std::string objectName = "Spawner_" + std::to_string(levelObjects.size() + 1);
			auto newObject = std::make_unique<GameObject>(objectName);
			auto transformComp = newObject->AddComponent<TransformComponent>();

			// カメラの少し前に配置するなど、出しやすい位置に設定
			transformComp->transform.translate = camera->GetTranslate();
			transformComp->transform.translate.z += 10.0f;
			transformComp->transform.rotate = { 0.0f, 0.0f, 0.0f };
			transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

			// エディタ上で視認するためのダミーモデル（例: "cube.obj"）をセット
			auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel("cube.gltf");
			auto* spawner = newObject->AddComponent<EnemySpawnerComponent>();
			spawner->Configure({}, spawnDistance, railCamera.get());

			newObject->Initialize();
			selectedObject = newObject.get(); // 生成してすぐ選択状態に
			levelObjects.push_back(std::move(newObject));

			// LevelData に「SPAWNER」として登録
			ObjectData newObjectData;
			newObjectData.type = "SPAWNER"; // ★ タイプを SPAWNER にする
			newObjectData.name = objectName;
			// file_name に「どの敵を出すか」の情報を間借りして保存するのもオススメです
			newObjectData.file_name = "EnemyTypeA";
			newObjectData.transform = transformComp->transform;
			newObjectData.components = { "ModelRenderer", "EnemySpawner" };

			level->GetLevelData()->objects.push_back(newObjectData);
		}

		ImGui::End();
	}
	// ★ ドラッグ操作中（マウスで何かを掴んでいる時）だけドロップ処理を有効化する
	if (ImGui::GetDragDropPayload() != nullptr) {

		ImGui::SetNextWindowPos(gameViewWindowPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(gameViewWindowSize, ImGuiCond_Always);
		ImGui::Begin("ViewportDropTarget", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoBringToFrontOnFocus
		);

		// 中央のゲーム画面だけをドロップ先にする。
		ImGui::InvisibleButton("##ViewportDropArea", gameViewWindowSize);

		// ドロップ判定
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_FILE")) {
				std::string droppedFileName = (const char*)payload->Data;

				// 1. 新しい GameObject を生成
				const std::string objectName = "SpawnedObject_" + std::to_string(levelObjects.size() + 1);
				auto newObject = std::make_unique<GameObject>(objectName);

				// 2. TransformComponent の追加と配置設定
				auto transformComp = newObject->AddComponent<TransformComponent>();
				Vector3 spawnPos = cameraTransform.translate;
				transformComp->transform.translate = { spawnPos.x, spawnPos.y, spawnPos.z + 10.0f };
				transformComp->transform.rotate = { 0.0f, 0.0f, 0.0f };
				transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

				// 3. ModelRendererComponent の追加とモデル設定
				auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
				modelRenderer->SetModel(droppedFileName);

				// 4. 初期化
				newObject->Initialize();

				// 5. 選択対象を生成したオブジェクトにする
				selectedObject = newObject.get();

				// 6. ゲームシーンの更新・描画リストに追加
				levelObjects.push_back(std::move(newObject));

				// 7. JSON保存用の LevelData にも新しいデータを登録
				ObjectData newObjectData;
				newObjectData.type = "MESH";
				newObjectData.name = objectName;
				newObjectData.file_name = droppedFileName;
				newObjectData.transform = transformComp->transform;
				newObjectData.components = { "ModelRenderer" };

				level->GetLevelData()->objects.push_back(newObjectData);

				OutputDebugStringA(("★★★ Spawned: " + droppedFileName + "\n").c_str());
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}

	// --- インスペクター ウィンドウ ---
	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Inspector")) {
		ImGui::SetNextWindowPos(ImVec2(editorIO.DisplaySize.x - editorRightPaneWidth, editorTopBarHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(editorRightPaneWidth, (std::max)(100.0f, editorIO.DisplaySize.y - editorTopBarHeight)), ImGuiCond_FirstUseEver);
		ImGui::Begin("Inspector");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Inspector");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Inspector");

		if (selectedObject != nullptr) {
			// ★ 選択中のオブジェクトが levelObjects の何番目かを検索する
			int selectedIndex = -1;
			for (size_t i = 0; i < levelObjects.size(); ++i) {
				if (levelObjects[i].get() == selectedObject) {
					selectedIndex = (int)i;
					break;
				}
			}

			// ★ インデックスが見つかり、かつ LevelData の範囲内であれば情報を表示
			if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
				std::string objType = level->GetLevelData()->objects[selectedIndex].type;
				std::string objName = level->GetLevelData()->objects[selectedIndex].name;
				std::string objFile = level->GetLevelData()->objects[selectedIndex].file_name;

				char nameBuffer[256]{};
				strncpy_s(nameBuffer, selectedObject->GetName().c_str(), _TRUNCATE);
				if (ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) {
					selectedObject->SetName(nameBuffer);
					level->GetLevelData()->objects[selectedIndex].name = nameBuffer;
				}

				bool isActive = selectedObject->IsActive();
				if (ImGui::Checkbox("Active", &isActive)) {
					selectedObject->SetActive(isActive);
				}

				ImGui::Text("Type: %s", objType.c_str());
				if (!objFile.empty()) {
					ImGui::Text("File: %s", objFile.c_str());
				}
			} else {
				ImGui::TextUnformatted("Type: Runtime GameObject");
			}

			ImGui::Separator();

			// Transform は必須コンポーネント。Unity の Inspector と同じように常に表示する。
			auto transformComp = selectedObject->GetTransform();
			if (transformComp && ImGui::CollapsingHeader(selectedObject->GetComponent<RectTransformComponent>() ? "Transform (3D - unused by Sprite)" : "Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat3("Position", &transformComp->transform.translate.x, 0.1f);
				ImGui::DragFloat3("Rotation", &transformComp->transform.rotate.x, 0.05f);
				ImGui::DragFloat3("Scale", &transformComp->transform.scale.x, 0.1f);
			}

			if (auto* rectTransform = selectedObject->GetComponent<RectTransformComponent>()) {
				if (ImGui::CollapsingHeader("Rect Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (ImGui::DragFloat2("Position (px)", &rectTransform->position.x, 1.0f)) {
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].rectPosition = rectTransform->position;
					}
					if (ImGui::DragFloat("Rotation (rad)", &rectTransform->rotation, 0.01f)) {
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].rectRotation = rectTransform->rotation;
					}
					if (ImGui::DragFloat2("Scale", &rectTransform->scale.x, 0.01f, 0.01f, 100.0f)) {
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].rectScale = rectTransform->scale;
					}
				}
			}

			// 任意コンポーネントは有効状態と主要な設定を個別に編集する。
			if (auto* renderer = selectedObject->GetComponent<ModelRendererComponent>()) {
				if (ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = renderer->IsEnabled();
					if (ImGui::Checkbox("Enabled##ModelRenderer", &enabled)) {
						renderer->SetEnabled(enabled);
					}

					char modelPath[260]{};
					strncpy_s(modelPath, renderer->GetModelPath().c_str(), _TRUNCATE);
					if (ImGui::InputText("Model", modelPath, IM_ARRAYSIZE(modelPath))) {
						renderer->SetModel(modelPath);
						if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
							level->GetLevelData()->objects[selectedIndex].file_name = modelPath;
						}
					}
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_FILE")) {
							const std::string droppedModel = static_cast<const char*>(payload->Data);
							renderer->SetModel(droppedModel);
							if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
								level->GetLevelData()->objects[selectedIndex].file_name = droppedModel;
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
			}

			if (auto* spriteRenderer = selectedObject->GetComponent<SpriteRendererComponent>()) {
				if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = spriteRenderer->IsEnabled();
					if (ImGui::Checkbox("Enabled##SpriteRenderer", &enabled)) {
						spriteRenderer->SetEnabled(enabled);
					}

					char texturePath[260]{};
					strncpy_s(texturePath, spriteRenderer->GetTexturePath().c_str(), _TRUNCATE);
					if (ImGui::InputText("Texture", texturePath, IM_ARRAYSIZE(texturePath))) {
						spriteRenderer->SetTexture(texturePath);
						if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
							level->GetLevelData()->objects[selectedIndex].sprite_file_name = texturePath;
						}
					}
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TEXTURE_FILE")) {
							const std::string droppedTexture = static_cast<const char*>(payload->Data);
							spriteRenderer->SetTexture(droppedTexture);
							if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
								level->GetLevelData()->objects[selectedIndex].sprite_file_name = droppedTexture;
							}
						}
						ImGui::EndDragDropTarget();
					}

					Vector2 spriteSize = spriteRenderer->GetSize();
					if (ImGui::DragFloat2("Size", &spriteSize.x, 1.0f, 1.0f, 4096.0f)) {
						spriteRenderer->SetSize(spriteSize);
					}
					Vector3 emissiveColor = spriteRenderer->GetEmissiveColor();
					float emissiveIntensity = spriteRenderer->GetEmissiveIntensity();
					bool emissiveChanged = ImGui::ColorEdit3("Emissive Color##SpriteRenderer", &emissiveColor.x);
					emissiveChanged |= ImGui::DragFloat("Emissive Intensity##SpriteRenderer", &emissiveIntensity, 0.05f, 0.0f, 10.0f);
					if (emissiveChanged) {
						spriteRenderer->SetEmissive(emissiveColor, emissiveIntensity);
						if (selectedIndex != -1) {
							auto& objectData = level->GetLevelData()->objects[selectedIndex];
							objectData.spriteEmissiveColor = emissiveColor;
							objectData.spriteEmissiveIntensity = emissiveIntensity;
						}
					}
				}
			}

			if (auto* textRenderer = selectedObject->GetComponent<TextRendererComponent>()) {
				if (ImGui::CollapsingHeader("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = textRenderer->IsEnabled();
					if (ImGui::Checkbox("Enabled##TextRenderer", &enabled)) {
						textRenderer->SetEnabled(enabled);
					}

					char textBuffer[1024]{};
					strncpy_s(textBuffer, textRenderer->GetText().c_str(), _TRUNCATE);
					if (ImGui::InputTextMultiline("Text", textBuffer, IM_ARRAYSIZE(textBuffer), ImVec2(-1.0f, 72.0f))) {
						textRenderer->SetText(textBuffer);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].text = textBuffer;
					}

					char fontFamilyBuffer[256]{};
					strncpy_s(fontFamilyBuffer, textRenderer->GetFontFamilyUtf8().c_str(), _TRUNCATE);
					if (ImGui::InputText("Font Family", fontFamilyBuffer, IM_ARRAYSIZE(fontFamilyBuffer))) {
						textRenderer->SetFontFamilyUtf8(fontFamilyBuffer);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textFontFamily = fontFamilyBuffer;
					}
					const std::string currentFontFamily = textRenderer->GetFontFamilyUtf8();
					const auto& fontPresets = TextRendererComponent::GetInstalledFontFamilies();
					if (ImGui::BeginCombo("Font Preset", currentFontFamily.c_str())) {
						for (const auto& fontPreset : fontPresets) {
							const bool selected = currentFontFamily == fontPreset;
							if (ImGui::Selectable(fontPreset.c_str(), selected)) {
								textRenderer->SetFontFamilyUtf8(fontPreset);
								if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textFontFamily = fontPreset;
							}
							if (selected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					bool bold = textRenderer->IsBold();
					if (ImGui::Checkbox("Bold", &bold)) {
						textRenderer->SetBold(bold);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textBold = bold;
					}
					bool outlineEnabled = textRenderer->IsOutlineEnabled();
					if (ImGui::Checkbox("Outline", &outlineEnabled)) {
						textRenderer->SetOutlineEnabled(outlineEnabled);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textOutlineEnabled = outlineEnabled;
					}
					if (outlineEnabled) {
						float outlineThickness = textRenderer->GetOutlineThickness();
						if (ImGui::DragFloat("Outline Thickness", &outlineThickness, 0.1f, 0.0f, 10.0f)) {
							textRenderer->SetOutlineThickness(outlineThickness);
							if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textOutlineThickness = outlineThickness;
						}
						Vector4 outlineColor = textRenderer->GetOutlineColor();
						if (ImGui::ColorEdit4("Outline Color", &outlineColor.x)) {
							textRenderer->SetOutlineColor(outlineColor);
							if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textOutlineColor = outlineColor;
						}
					}

					Vector3 emissiveColor = textRenderer->GetEmissiveColor();
					float emissiveIntensity = textRenderer->GetEmissiveIntensity();
					bool emissiveChanged = ImGui::ColorEdit3("Emissive Color", &emissiveColor.x);
					emissiveChanged |= ImGui::DragFloat("Emissive Intensity", &emissiveIntensity, 0.05f, 0.0f, 10.0f);
					if (emissiveChanged) {
						textRenderer->SetEmissive(emissiveColor, emissiveIntensity);
						if (selectedIndex != -1) {
							auto& objectData = level->GetLevelData()->objects[selectedIndex];
							objectData.textEmissiveColor = emissiveColor;
							objectData.textEmissiveIntensity = emissiveIntensity;
						}
					}
					float characterSpacing = textRenderer->GetCharacterSpacing();
					if (ImGui::DragFloat("Character Spacing", &characterSpacing, 0.1f, -10.0f, 50.0f)) {
						textRenderer->SetCharacterSpacing(characterSpacing);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textCharacterSpacing = characterSpacing;
					}

					int fontSize = textRenderer->GetFontSize();
					if (ImGui::DragInt("Font Size", &fontSize, 1.0f, 1, 256)) {
						textRenderer->SetFontSize(fontSize);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textFontSize = fontSize;
					}
					float maxWidth = textRenderer->GetMaxWidth();
					if (ImGui::DragFloat("Max Width (0 = no wrap)", &maxWidth, 1.0f, 0.0f, 1920.0f)) {
						textRenderer->SetMaxWidth(maxWidth);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textMaxWidth = maxWidth;
					}
					Vector4 color = textRenderer->GetColor();
					if (ImGui::ColorEdit4("Color##TextRenderer", &color.x)) {
						textRenderer->SetColor(color);
						if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textColor = color;
					}
				}
			}

			if (auto* animatorComponent = selectedObject->GetComponent<AnimatorComponent>()) {
				if (ImGui::CollapsingHeader("Animator")) {
					bool enabled = animatorComponent->IsEnabled();
					if (ImGui::Checkbox("Enabled##Animator", &enabled)) {
						animatorComponent->SetEnabled(enabled);
					}
				}
			}

			if (auto* railPoint = selectedObject->GetComponent<RailPointComponent>()) {
				if (ImGui::CollapsingHeader("Rail Camera Point")) {
					bool enabled = railPoint->IsEnabled();
					if (ImGui::Checkbox("Enabled##RailPoint", &enabled)) {
						railPoint->SetEnabled(enabled);
					}
				}
			}

			if (auto* spawner = selectedObject->GetComponent<EnemySpawnerComponent>()) {
				if (ImGui::CollapsingHeader("Enemy Spawner")) {
					bool enabled = spawner->IsEnabled();
					if (ImGui::Checkbox("Enabled##EnemySpawner", &enabled)) {
						spawner->SetEnabled(enabled);
					}
				}
			}
			if (auto* collider = selectedObject->GetComponent<ColliderComponent>()) {
				if (selectedIndex != -1) {
					LevelEditorCommon::DrawColliderInspector(*collider, level->GetLevelData()->objects[selectedIndex]);
				}
			}

			if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size() &&
				ImGui::CollapsingHeader("Add Component")) {
				auto& objectData = level->GetLevelData()->objects[selectedIndex];
				auto addSerializedComponent = [&objectData](const char* componentName) {
					if (std::find(objectData.components.begin(), objectData.components.end(), componentName) == objectData.components.end()) {
						objectData.components.push_back(componentName);
					}
					};

				if (!selectedObject->GetComponent<ModelRendererComponent>() && ImGui::Button("Model Renderer")) {
					auto* renderer = selectedObject->AddComponent<ModelRendererComponent>();
					renderer->SetModel("cube.gltf");
					objectData.file_name = "cube.gltf";
					addSerializedComponent("ModelRenderer");
				}
				if (!selectedObject->GetComponent<SpriteRendererComponent>() && ImGui::Button("Sprite Renderer")) {
					auto* rectTransform = selectedObject->AddComponent<RectTransformComponent>();
					rectTransform->position = { 960.0f, 540.0f };
					auto* spriteRenderer = selectedObject->AddComponent<SpriteRendererComponent>();
					spriteRenderer->SetTexture("Resource/title/title.png");
					objectData.sprite_file_name = "Resource/title/title.png";
					objectData.spriteEmissiveColor = { 1.0f, 1.0f, 1.0f };
					objectData.spriteEmissiveIntensity = 0.0f;
					objectData.rectPosition = rectTransform->position;
					objectData.rectRotation = rectTransform->rotation;
					objectData.rectScale = rectTransform->scale;
					addSerializedComponent("RectTransform");
					addSerializedComponent("SpriteRenderer");
				}
				if (!selectedObject->GetComponent<TextRendererComponent>() && ImGui::Button("Text Renderer")) {
					auto* rectTransform = selectedObject->GetComponent<RectTransformComponent>();
					if (!rectTransform) {
						rectTransform = selectedObject->AddComponent<RectTransformComponent>();
						rectTransform->position = { 960.0f, 540.0f };
					}
					auto* textRenderer = selectedObject->AddComponent<TextRendererComponent>();
					textRenderer->SetText("New Text");
					textRenderer->SetFontFamilyUtf8("Meiryo UI");
					textRenderer->SetFontSize(32);
					objectData.text = "New Text";
					objectData.textFontFamily = "Meiryo UI";
					objectData.textFontSize = 32;
					objectData.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					objectData.textMaxWidth = 0.0f;
					objectData.textBold = false;
					objectData.textOutlineEnabled = false;
					objectData.textOutlineThickness = 1.0f;
					objectData.textOutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
					objectData.textEmissiveColor = { 1.0f, 1.0f, 1.0f };
					objectData.textEmissiveIntensity = 0.0f;
					objectData.textCharacterSpacing = 0.0f;
					objectData.rectPosition = rectTransform->position;
					objectData.rectRotation = rectTransform->rotation;
					objectData.rectScale = rectTransform->scale;
					addSerializedComponent("RectTransform");
					addSerializedComponent("TextRenderer");
				}
				if (!selectedObject->GetComponent<RailPointComponent>() && ImGui::Button("Rail Camera Point")) {
					selectedObject->AddComponent<RailPointComponent>();
					addSerializedComponent("RailPoint");
				}
				if (!selectedObject->GetComponent<EnemySpawnerComponent>() && ImGui::Button("Enemy Spawner")) {
					auto* spawner = selectedObject->AddComponent<EnemySpawnerComponent>();
					spawner->Configure(objectData.spawnDataList, spawnDistance, railCamera.get());
					addSerializedComponent("EnemySpawner");
				}
				if ((objectData.type == "EMPTY" || objectData.type == "empty") &&
					!selectedObject->GetComponent<ColliderComponent>() && ImGui::Button("Collider")) {
					auto* collider = selectedObject->AddComponent<ColliderComponent>();
					objectData.colliderSize = collider->size;
					objectData.colliderCenterOffset = collider->centerOffset;
					addSerializedComponent("Collider");
				}
				if (!selectedObject->GetComponent<Enemy>() && ImGui::Button("Enemy Normal")) {
					auto* enemy = selectedObject->AddComponent<EnemyNormal>();
					enemy->SetTransform(selectedObject->GetTransform()->transform);
					addSerializedComponent("EnemyNormal");
				}
			}

			// オブジェクト削除ボタン（誤誤爆防止のために赤色スタイル適用）
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

			if (selectedIndex != -1 && ImGui::Button("Delete Object", ImVec2(-1, 0))) {
				if (selectedIndex != -1) {
					level->GetLevelData()->objects.erase(level->GetLevelData()->objects.begin() + selectedIndex);
					levelObjects.erase(levelObjects.begin() + selectedIndex);
				}
				selectedObject = nullptr;
				ImGui::PopStyleColor(3);
				ImGui::End();
				return;
			}
			ImGui::PopStyleColor(3);

			// =========================================================================
			// SPAWNER 専用のタイムライン編集 UI
			// =========================================================================
			if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
				// 参照として取得し、直接書き換えられるようにする
				ObjectData& currentObjData = level->GetLevelData()->objects[selectedIndex];

				if (auto* spawnerComponent = selectedObject->GetComponent<EnemySpawnerComponent>()) {
					ImGui::Separator();
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "◆ Spawner Settings");
					if (ImGui::Button("Apply Spawner Settings")) {
						spawnerComponent->SetSpawnList(currentObjData.spawnDataList);
					}

					if (ImGui::Button("Add Enemy to Spawner")) {
						// 初期値として追加
						currentObjData.spawnDataList.push_back({ 0.0f, { 0.0f, 0.0f, 0.0f }, "NORMAL" });
					}

					ImGui::Spacing();

					// プルダウン（Combo）に表示する敵の種類のリスト
					const char* enemyTypes[] = { "NORMAL", "FAST", "BOSS" };
					// プルダウン（Combo）に表示する移動パターンのリスト
					const char* movePatterns[] = { "IDRE", "STRAIGHT", "WAVE", "HOMING", "PATH", "RAIL_FORWARD" };

					// 登録されている敵のリストをループ表示
					for (size_t i = 0; i < currentObjData.spawnDataList.size(); ++i) {
						ImGui::PushID(static_cast<int>(i));
						auto& sData = currentObjData.spawnDataList[i];

						ImGui::Text("Enemy [%zu]", i);

						// 1. 出現タイミング
						ImGui::DragFloat("Spawn Time(s)", &sData.spawnTime, 0.1f, 0.0f, 100.0f);
						// 2. 座標オフセット
						ImGui::DragFloat3("Offset XYZ", &sData.offset.x, 0.1f);

						// 3. 敵のタイプ（Comboボックス）
						// 現在の sData.type が、配列の何番目と一致するかを探す
						int currentTypeIndex = 0;
						for (int j = 0; j < IM_ARRAYSIZE(enemyTypes); j++) {
							if (sData.type == enemyTypes[j]) {
								currentTypeIndex = j;
								break;
							}
						}

						// プルダウンのUIを表示し、変更があったら sData.type に文字列を書き戻す
						if (ImGui::Combo("Type", &currentTypeIndex, enemyTypes, IM_ARRAYSIZE(enemyTypes))) {
							sData.type = enemyTypes[currentTypeIndex];
						}

						// 2. ★移動パターンの選択 (PATH を選択すると制御点編集が展開)
						int currentPatternIndex = 0;
						for (int j = 0; j < IM_ARRAYSIZE(movePatterns); j++) {
							if (sData.movePattern == movePatterns[j]) { currentPatternIndex = j; break; }
						}
						if (ImGui::Combo("Move Pattern", &currentPatternIndex, movePatterns, IM_ARRAYSIZE(movePatterns))) {
							sData.movePattern = movePatterns[currentPatternIndex];
						}

						// 3. ★移動パターンが "PATH" の場合のみ制御点(controlPoints)の編集UIを表示
						if (sData.movePattern == "PATH") {
							if (ImGui::TreeNode("Path Control Points")) {

								// 制御点追加ボタン
								if (ImGui::Button("+ Add Control Point")) {
									Vector3 newPoint;

									if (sData.controlPoints.empty()) {
										// 制御点が0個の場合：スポナーの座標 ＋ エネミーのオフセット をワールド座標として算出
										newPoint.x = currentObjData.transform.translate.x + sData.offset.x;
										newPoint.y = currentObjData.transform.translate.y + sData.offset.y;
										newPoint.z = currentObjData.transform.translate.z + sData.offset.z;
									} else {
										// 既に制御点がある場合：最後の制御点をベースにする
										newPoint = sData.controlPoints.back();
									}

									sData.controlPoints.push_back(newPoint);
								}

								// 各制御点の編集と削除
								for (size_t cpIdx = 0; cpIdx < sData.controlPoints.size(); ++cpIdx) {
									ImGui::PushID(static_cast<int>(cpIdx));

									// 座標調整ドラッグバー
									std::string label = "P[" + std::to_string(cpIdx) + "]";
									ImGui::DragFloat3(label.c_str(), &sData.controlPoints[cpIdx].x, 0.1f);

									// 制御点削除ボタン
									ImGui::SameLine();
									if (ImGui::Button("Delete")) {
										sData.controlPoints.erase(sData.controlPoints.begin() + cpIdx);
										ImGui::PopID();
										break;
									}

									ImGui::PopID();
								}
								ImGui::TreePop();
							}
						}

						// 4. 削除ボタン
						if (ImGui::Button("Delete")) {
							currentObjData.spawnDataList.erase(currentObjData.spawnDataList.begin() + i);
							ImGui::PopID();
							break; // リストのサイズが変わるのでループを抜ける
						}

						ImGui::Separator();
						ImGui::PopID();
					}
				}
			}


		} else {
			// オブジェクトが選択されていない時の表示
			ImGui::TextDisabled("No object selected.");
		}

		ImGui::End();
	}
	if (!ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Settings")) {
		// Settings が別タブでも、Game 上のギズモは常に操作できるようにする。
		GizmoUpdate(false);
	}
	ImGuiFunction::GetInstance()->UpdateDockingGuide();

#endif

}

void TitleScene::Draw2D() {
	// 2Dオブジェクトの描画準備
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	// スプライト描画
	for (const auto& levelObject : levelObjects) {
		if (auto* spriteRenderer = levelObject->GetComponent<SpriteRendererComponent>()) {
			if (levelObject->IsActive() && spriteRenderer->IsEnabled()) {
				spriteRenderer->DrawSprite();
			}
		}
		if (auto* textRenderer = levelObject->GetComponent<TextRendererComponent>()) {
			if (levelObject->IsActive() && textRenderer->IsEnabled()) {
				textRenderer->DrawTextSprite();
			}
		}
	}
}
void TitleScene::Draw3D() {
	// スカイボックス
	//Skybox::GetInstance()->Draw();

	// 3Dオブジェクトの描画準備
	ObjectCommon::GetInstance()->SetCommonPipelineState();

	// レベルオブジェクト
	for (auto& object : levelObjects) {
		object->Draw();
	}
	// スポナーの敵出現プレビューの描画
	for (auto& preview : spawnerPreviewObjects) {
		preview->Draw();
	}
	// 敵の移動制御点プレビューの描画
	for (auto& preview : pathPreviewObjects) {
		preview->Draw();
	}

	// 敵の描画
	for (auto& enemyObject : enemies) {
		enemyObject->Draw();
	}

#ifdef _DEBUG
	// 1. Line専用のグラフィックスパイプラインとルートシグネイチャを設定
	LineCommon::GetInstance()->SetCommonPipelineState(); //[cite: 18, 19]

	// 2. 蓄積された線の描画コマンドを発行
	debugLineNormal->Update();
	debugLineNormal->Draw();

	debugLineHit->Update();
	debugLineHit->Draw();

#endif

	railCamera->EditorDraw();

	// 3Dオブジェクト描画
	//for (int i = 0; i < 2; i++) {
	//	object[i]->Draw();
	//}


	// アウトライン描画準備
	ObjectCommon::GetInstance()->SetOutlinePipelineState();

	// アウトライン描画
	//for (int i = 0; i < 2; i++) {
	//	object[i]->Draw();
	//}

}

void TitleScene::Finalize() {
	CameraManager::GetInstance()->RemoveCamera("main");
}

void TitleScene::CreateLevel() {
	for (auto& objectData : level->GetLevelData()->objects) {
		if (objectData.type == "MESH" || objectData.type == "mesh") {
			// 1. GameObject の生成
			auto gameObject = std::make_unique<GameObject>(objectData.name);

			// 2. TransformComponent を追加してトランスフォーム情報をセット
			auto transform = gameObject->AddComponent<TransformComponent>();
			transform->transform.translate = objectData.transform.translate;
			transform->transform.rotate = objectData.transform.rotate;
			transform->transform.scale = objectData.transform.scale;

			// 3. ModelRendererComponent を追加して 3D モデルをセット
			auto modelRenderer = gameObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel(objectData.file_name);

			// 4. アタッチされたコンポーネントを一括初期化
			gameObject->Initialize();

			// 5. コンテナに登録
			levelObjects.push_back(std::move(gameObject));

			std::string debugMsg = "LevelObject File: [" + objectData.file_name + "]\n";
			OutputDebugStringA(debugMsg.c_str());
		} else if (objectData.type == "RAIL" || objectData.type == "rail") {
			// ★ JSONから読み込んだ座標と回転を RailCamera の制御点として追加
			railCamera->AddPoint(objectData.transform.translate, objectData.transform.rotate);

			// ★ ここから追加: 実体のGameObjectを生成してシーンに配置する
			auto gameObject = std::make_unique<GameObject>(objectData.name);
			auto transform = gameObject->AddComponent<TransformComponent>();

			transform->transform.translate = objectData.transform.translate;
			transform->transform.rotate = objectData.transform.rotate;
			transform->transform.scale = objectData.transform.scale;

			// モデルのセット (JSONにファイル名がない場合は "rail.obj" をデフォルトにする)
			auto modelRenderer = gameObject->AddComponent<ModelRendererComponent>();
			std::string modelName = objectData.file_name.empty() ? "rail.obj" : objectData.file_name;
			modelRenderer->SetModel(modelName);
			gameObject->AddComponent<RailPointComponent>();

			gameObject->Initialize();
			levelObjects.push_back(std::move(gameObject));

		} else if (objectData.type == "SPAWNER" || objectData.type == "spawner") {
			// SPAWNER の実体オブジェクトを生成して levelObjects に登録する
			auto gameObject = std::make_unique<GameObject>(objectData.name);

			auto transform = gameObject->AddComponent<TransformComponent>();
			transform->transform.translate = objectData.transform.translate;
			transform->transform.rotate = objectData.transform.rotate;
			transform->transform.scale = objectData.transform.scale;

			// エディタ表示用のモデルをアタッチ
			auto modelRenderer = gameObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel("cube.gltf");

			// ゲームロジック用のスポナーも同じ GameObject にアタッチする。
			auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
			spawner->Configure(objectData.spawnDataList, spawnDistance, railCamera.get());

			gameObject->Initialize();
			levelObjects.push_back(std::move(gameObject));
		} else if (objectData.type == "EMPTY" || objectData.type == "empty") {
			// Empty は Transform だけを持つ、Hierarchy 用の GameObject。
			auto gameObject = std::make_unique<GameObject>(objectData.name);
			gameObject->GetTransform()->transform = objectData.transform;

			const auto hasComponent = [&objectData](const char* componentName) {
				return std::find(objectData.components.begin(), objectData.components.end(), componentName) != objectData.components.end();
				};
			if (hasComponent("ModelRenderer")) {
				auto* renderer = gameObject->AddComponent<ModelRendererComponent>();
				renderer->SetModel(objectData.file_name.empty() ? "cube.gltf" : objectData.file_name);
			}
			if (hasComponent("RectTransform")) {
				auto* rectTransform = gameObject->AddComponent<RectTransformComponent>();
				rectTransform->position = objectData.rectPosition;
				rectTransform->rotation = objectData.rectRotation;
				rectTransform->scale = objectData.rectScale;
			}
			if (hasComponent("Collider")) {
				auto* collider = gameObject->AddComponent<ColliderComponent>();
				collider->size = objectData.colliderSize;
				collider->centerOffset = objectData.colliderCenterOffset;
			}
			if (hasComponent("SpriteRenderer")) {
				if (!gameObject->GetComponent<RectTransformComponent>()) {
					gameObject->AddComponent<RectTransformComponent>();
				}
				auto* spriteRenderer = gameObject->AddComponent<SpriteRendererComponent>();
				spriteRenderer->SetTexture(objectData.sprite_file_name.empty() ? "Resource/title/title.png" : objectData.sprite_file_name);
				spriteRenderer->SetEmissive(objectData.spriteEmissiveColor, objectData.spriteEmissiveIntensity);
			}
			if (hasComponent("TextRenderer")) {
				if (!gameObject->GetComponent<RectTransformComponent>()) {
					gameObject->AddComponent<RectTransformComponent>();
				}
				auto* textRenderer = gameObject->AddComponent<TextRendererComponent>();
				textRenderer->SetText(objectData.text);
				textRenderer->SetFontFamilyUtf8(objectData.textFontFamily);
				textRenderer->SetFontSize(objectData.textFontSize);
				textRenderer->SetColor(objectData.textColor);
				textRenderer->SetMaxWidth(objectData.textMaxWidth);
				textRenderer->SetBold(objectData.textBold);
				textRenderer->SetOutlineEnabled(objectData.textOutlineEnabled);
				textRenderer->SetOutlineThickness(objectData.textOutlineThickness);
				textRenderer->SetOutlineColor(objectData.textOutlineColor);
				textRenderer->SetEmissive(objectData.textEmissiveColor, objectData.textEmissiveIntensity);
				textRenderer->SetCharacterSpacing(objectData.textCharacterSpacing);
			}
			if (hasComponent("RailPoint")) {
				gameObject->AddComponent<RailPointComponent>();
			}
			if (hasComponent("EnemySpawner")) {
				auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
				spawner->Configure(objectData.spawnDataList, spawnDistance, railCamera.get());
			}
			if (hasComponent("EnemyNormal")) {
				auto* enemy = gameObject->AddComponent<EnemyNormal>();
				enemy->SetTransform(objectData.transform);
			}
			gameObject->Initialize();
			levelObjects.push_back(std::move(gameObject));
		}

	}
}

void TitleScene::GizmoUpdate(bool showEditorControls) {
	auto input = Input::GetInstance();
	const ImGuiIO& editorIO = ImGui::GetIO();
	const ImVec2 gameWindowPosition(gameViewPosition.x, gameViewPosition.y);
	const ImVec2 gameWindowSize(gameViewSize.x, gameViewSize.y);

	if (showEditorControls) {
		// 操作モードの切り替えラジオボタン
		ImGui::Text("Gizmo Operation");
		if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
			currentGizmoOperation = ImGuizmo::TRANSLATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
			currentGizmoOperation = ImGuizmo::ROTATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
			currentGizmoOperation = ImGuizmo::SCALE;
		}
	}

	// マウス左クリックの瞬間 ＆ ImGuizmoを操作中でない場合のみ判定
	const ImVec2 mousePosition = ImGui::GetMousePos();
	const bool isMouseInGameView =
		mousePosition.x >= gameWindowPosition.x && mousePosition.x < gameWindowPosition.x + gameWindowSize.x &&
		mousePosition.y >= gameWindowPosition.y && mousePosition.y < gameWindowPosition.y + gameWindowSize.y;
	// Game ウィンドウ自体も ImGui の入力を捕捉するため、領域内の操作はゲーム入力として許可する。
	const bool isEditorPanelHovered = editorIO.WantCaptureMouse && !isMouseInGameView;
	if (ImGui::IsMouseClicked(0) && isMouseInGameView && !ImGuizmo::IsOver() && !isEditorPanelHovered) {
		Vector2 mousePos = input->GetMouseScreen(); // ※ご自身のInputクラスの関数に合わせる

		float windowWidth = 1920.0f; // 画面幅
		float windowHeight = 1080.0f; // 画面高さ
		// Game ウィンドウ内のマウス座標を、元のレンダーターゲット座標へ戻す。
		mousePos.x = (mousePos.x - gameWindowPosition.x) * windowWidth / gameWindowSize.x;
		mousePos.y = (mousePos.y - gameWindowPosition.y) * windowHeight / gameWindowSize.y;

		// Rayを生成
		Ray ray = ScreenToRay(mousePos, windowWidth, windowHeight, camera->GetViewMatrix(), camera->GetProjectionMatrix());

		GameObject* closestObject = nullptr;
		float closestDistance = (std::numeric_limits<float>::max)();

		// レベル内の全オブジェクトと当たり判定
		for (auto& obj : levelObjects) {
			auto transformComp = obj->GetComponent<TransformComponent>();
			if (transformComp) {
				Vector3 center = transformComp->transform.translate;
				// スケールの最大値を半径の目安にする
				float baseModelSize = 5.0f;
				float radius = (std::max)({ transformComp->transform.scale.x, transformComp->transform.scale.y, transformComp->transform.scale.z }) * baseModelSize;

				// ★既存のRaySphereIntersectを使用
				float hitDistance = RaySphereIntersect(ray.origin, ray.direction, center, radius);

				// hitDistance が 0以上なら当たっている (交差なしの場合は -1.0f が返る)
				if (hitDistance >= 0.0f) {
					// 複数重なっている場合は、距離が一番近い(手前にある)ものを選択
					if (hitDistance < closestDistance) {
						closestDistance = hitDistance;
						closestObject = obj.get();
					}
				}
			}
		}

		// 選択対象を更新
		selectedObject = closestObject;
	}

	// --- ImGuizmoの処理 ---

	// ★修正1: ImGuizmoに入力の受付を開始させる（毎フレーム必ず呼ぶ必要があります）
	ImGuizmo::BeginFrame();

	if (selectedObject) {
		auto transformComp = selectedObject->GetComponent<TransformComponent>();
		if (transformComp) {
			auto* rectTransform = selectedObject->GetComponent<RectTransformComponent>();
			const bool isSpriteObject = rectTransform != nullptr;
			// 1. ImGuizmoの初期設定
			ImGuizmo::SetOrthographic(isSpriteObject);

			// 中央の Game ビューにだけギズモを描画・判定する。
			// Foreground の描画リストは Game ウィンドウ自身のものではないため、
			// ImGuizmo に入力判定対象のウィンドウを明示的に渡す。
			ImGuizmo::SetAlternativeWindow(ImGui::FindWindowByName("Game"));
			ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

			ImGuizmo::SetRect(gameWindowPosition.x, gameWindowPosition.y, gameWindowSize.x, gameWindowSize.y);

			// ★追加：UI上にマウスがある、かつギズモをドラッグ中でない場合はギズモの操作を無効化する
			bool isUIHovered = editorIO.WantCaptureMouse && !isMouseInGameView;
			bool isGizmoDragging = ImGuizmo::IsUsing();
			ImGuizmo::Enable(!isUIHovered || isGizmoDragging);

			// 2. カメラの行列を取得
			Matrix4x4 viewMat = isSpriteObject ? MakeIdentity4x4() : camera->GetViewMatrix();
			Matrix4x4 projMat = isSpriteObject ? MakeOrthographicMatrix(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 100.0f) : camera->GetProjectionMatrix();
			Matrix4x4 worldMat = isSpriteObject
				? MakeAffineMatrix(Vector3{ rectTransform->scale.x, rectTransform->scale.y, 1.0f }, Vector3{ 0.0f, 0.0f, rectTransform->rotation }, Vector3{ rectTransform->position.x, rectTransform->position.y, 0.0f })
				: transformComp->GetWorldMatrix();

			// 動かした「差分」を受け取るための行列を用意
			Matrix4x4 deltaMat;

			// 3. ギズモの操作と行列の更新
			ImGuizmo::Manipulate(
				&viewMat.m[0][0],        // View行列のfloatポインタ
				&projMat.m[0][0],        // Projection行列のfloatポインタ
				currentGizmoOperation,   // 操作モード
				ImGuizmo::LOCAL,         // 座標系 (LOCAL or WORLD)
				&worldMat.m[0][0],       // World行列のfloatポインタ
				&deltaMat.m[0][0]       // 差分行列のfloatポインタ
			);

			// 4. ギズモによって操作が行われたら、TransformComponent に反映
			if (ImGuizmo::IsUsing()) {
				float translation[3] = { 0.0f };
				float rotation[3] = { 0.0f };
				float scale[3] = { 0.0f };

				if (isSpriteObject) {
					if (currentGizmoOperation == ImGuizmo::ROTATE) {
						ImGuizmo::DecomposeMatrixToComponents(&deltaMat.m[0][0], translation, rotation, scale);
						rectTransform->rotation += rotation[2] * (3.14159265f / 180.0f);
					} else {
						ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotation, scale);
						if (currentGizmoOperation == ImGuizmo::TRANSLATE) {
							rectTransform->position = { translation[0], translation[1] };
						} else if (currentGizmoOperation == ImGuizmo::SCALE) {
							rectTransform->scale = { scale[0], scale[1] };
						}
					}
				} else if (currentGizmoOperation == ImGuizmo::TRANSLATE) {
					// 移動は今まで通り全体の行列から取り出す
					ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotation, scale);
					transformComp->transform.translate = { translation[0], translation[1], translation[2] };
				} else if (currentGizmoOperation == ImGuizmo::ROTATE) {
					// ★回転の時だけ、全体の行列ではなく「差分(deltaMat)」を分解する！
					ImGuizmo::DecomposeMatrixToComponents(&deltaMat.m[0][0], translation, rotation, scale);

					// 差分(このフレームで動かした量)を、現在の角度に「足し算(+=)」する
					transformComp->transform.rotate.x += rotation[0] * (3.14159265f / 180.0f);
					transformComp->transform.rotate.y += rotation[1] * (3.14159265f / 180.0f);
					transformComp->transform.rotate.z += rotation[2] * (3.14159265f / 180.0f);
				} else if (currentGizmoOperation == ImGuizmo::SCALE) {
					// 拡縮も今まで通り全体の行列から取り出す
					ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotation, scale);
					transformComp->transform.scale = { scale[0], scale[1], scale[2] };
				}
			}
		}
	}

	if (!showEditorControls) {
		return;
	}

	// 敵の出現地点のプレビュー表示
	if (selectedObject != nullptr) {
		// 選択中オブジェクトのインデックスを探す
		int selectedIndex = -1;
		for (size_t i = 0; i < levelObjects.size(); ++i) {
			if (levelObjects[i].get() == selectedObject) {
				selectedIndex = (int)i;
				break;
			}
		}

		if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
			auto& currentObjData = level->GetLevelData()->objects[selectedIndex];

			// 選択中が SPAWNER の場合のみプレビューを生成・更新する
			if (selectedObject->GetComponent<EnemySpawnerComponent>()) {

				// プレビューオブジェクトの数が足りない場合は生成して追加
				while (spawnerPreviewObjects.size() < currentObjData.spawnDataList.size()) {
					auto previewObj = std::make_unique<GameObject>();
					previewObj->AddComponent<TransformComponent>();
					auto modelRenderer = previewObj->AddComponent<ModelRendererComponent>();

					// プレビュー用のモデル名（実際の敵モデルや半透明のモデルに変えてください）
					modelRenderer->SetModel("cube.gltf");

					previewObj->Initialize();
					spawnerPreviewObjects.push_back(std::move(previewObj));
				}

				// プレビューオブジェクトが多すぎる場合（敵を削除した時）は末尾を削除
				while (spawnerPreviewObjects.size() > currentObjData.spawnDataList.size()) {
					spawnerPreviewObjects.pop_back();
				}

				// 座標を「スポナーの座標 + 敵のオフセット座標」に更新
				auto spawnerTransform = selectedObject->GetComponent<TransformComponent>();
				for (size_t i = 0; i < currentObjData.spawnDataList.size(); ++i) {
					auto previewTransform = spawnerPreviewObjects[i]->GetComponent<TransformComponent>();

					// スポナーのワールド座標に offset を加算
					previewTransform->transform.translate.x = spawnerTransform->transform.translate.x + currentObjData.spawnDataList[i].offset.x;
					previewTransform->transform.translate.y = spawnerTransform->transform.translate.y + currentObjData.spawnDataList[i].offset.y;
					previewTransform->transform.translate.z = spawnerTransform->transform.translate.z + currentObjData.spawnDataList[i].offset.z;

					// 回転・スケールはスポナーに合わせる（または固定値）
					previewTransform->transform.rotate = spawnerTransform->transform.rotate;
					previewTransform->transform.scale = { 1.0f, 1.0f, 1.0f };

					// (応用) 敵の type に応じて表示モデルを切り替えることも可能です			
					auto renderer = spawnerPreviewObjects[i]->GetComponent<ModelRendererComponent>();

					if (currentObjData.spawnDataList[i].type == "NORMAL")
						renderer->SetModel("cube.gltf");
					if (currentObjData.spawnDataList[i].type == "FAST")
						renderer->SetModel("cube.gltf");
					if (currentObjData.spawnDataList[i].type == "BOSS")
						renderer->SetModel("cube.gltf");


					spawnerPreviewObjects[i]->Update();
				}

				// =========================================================
				// ★追加: 2. 移動パターンの制御点(PATH)プレビュー処理
				// =========================================================
				size_t totalPathPoints = 0; // 表示中の制御点の総数

				for (size_t i = 0; i < currentObjData.spawnDataList.size(); ++i) {
					auto& sData = currentObjData.spawnDataList[i];

					// 移動パターンが PATH の場合のみ制御点モデルを生成・配置
					if (sData.movePattern == "PATH") {
						for (size_t j = 0; j < sData.controlPoints.size(); ++j) {

							// オブジェクトが足りなければ追加生成
							if (pathPreviewObjects.size() <= totalPathPoints) {
								auto previewObj = std::make_unique<GameObject>();
								previewObj->AddComponent<TransformComponent>();
								auto modelRenderer = previewObj->AddComponent<ModelRendererComponent>();

								// 制御点用モデル（敵と区別しやすいように設定）
								modelRenderer->SetModel("cube.gltf");
								previewObj->Initialize();
								pathPreviewObjects.push_back(std::move(previewObj));
							}

							// 制御点のワールド座標を適用（サイズは半分に縮小）
							auto pathTransform = pathPreviewObjects[totalPathPoints]->GetComponent<TransformComponent>();
							pathTransform->transform.translate = sData.controlPoints[j];
							pathTransform->transform.scale = { 0.5f, 0.5f, 0.5f };

							pathPreviewObjects[totalPathPoints]->Update();
							totalPathPoints++;
						}
					}
				}

				// 不要になった（制御点が削除された）プレビューを削除
				while (pathPreviewObjects.size() > totalPathPoints) {
					pathPreviewObjects.pop_back();
				}

			} else {
				// SPAWNER以外を選択中の時はプレビューを消去
				spawnerPreviewObjects.clear();
				pathPreviewObjects.clear();
			}
		}
	} else {
		// 何も選択していない時もプレビューを消去
		spawnerPreviewObjects.clear();
		pathPreviewObjects.clear();
	}
}

