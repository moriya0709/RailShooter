#include "GamePlayScene.h"
#include "ObjectCommon.h"
#include "SpriteCommon.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "TransformComponent.h"
#include "ModelRendererComponent.h"
#include "AnimatorComponent.h"
#include "RailPointComponent.h"
#include "ObjectRailMovementComponent.h"
#include "EnemySpawnerComponent.h"
#include "EasingComponent.h"
#include "EnemyNormal.h"
#include "SpriteRendererComponent.h"
#include "TextRendererComponent.h"
#include "TextMeshEditorGizmo.h"
#include "RectTransformComponent.h"
#include "ColliderComponent.h"
#include "LevelEditorCommon.h"
#include "SkyBox.h"
#include "LineCommon.h"
#include "ModelManager.h"
#include <cstring>
#include <cctype>
#include <filesystem>
#include <iterator>

void GamePlayScene::Initialize() {

	railCamera = std::make_unique<RailCamera>();
	railCamera->Initialize();

	// レベル
	level = std::make_unique<Level>();
	level->LoadJson("gamePlayScene");
	// プリセットの読み込み
	level->LoadRailPresetsJson();
	activeRailGroupName_ = level->GetLevelData()->activeRailCameraGroup;
	CreateLevel();

	// Use an EMPTY + CameraComponent from the level. Keep the previous behavior
	// as a fallback for old levels without a Camera component.
	if (!camera) {
		fallbackCameraObject = std::make_unique<GameObject>("MainCamera");
		fallbackCameraObject->GetTransform()->transform = cameraTransform;
		camera = fallbackCameraObject->AddComponent<CameraComponent>();
		fallbackCameraObject->Initialize();
		cameraObject = fallbackCameraObject.get();
	}

	// シーンで定義した初期位置・回転
	camera->SetTranslate(cameraTransform.translate);
	camera->SetRotate(cameraTransform.rotate);

	// カメラマネージャ登録
	CameraManager::GetInstance()->AddCamera("main", camera->GetCamera());
	CameraManager::GetInstance()->SetActiveCamera("main");


	// 3Dオブジェクト
	for (int i = 0; i < 2; i++) {
		object[i] = std::make_unique <Object>();
		object[i]->Initialize(camera->GetCamera());
	}

	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object[0]->SetModel("cube.gltf");
	object[1]->SetModel("cube.gltf");

	// 音声再生
	//SoundManager::GetInstance()->Play("bgm");

	// タイマー
	gameTimer = std::make_unique<GameTimer>();

	// プレイヤー
	defaultPlayerObject = std::make_unique<GameObject>("Player");
	playerObject = defaultPlayerObject.get();
	player = playerObject->AddComponent<Player>();
	playerObject->Initialize();
	player->SetMoveEnabled(false); // プレイヤーの移動を無効化

	// 当たり判定の線
	debugLineNormal = std::make_unique<Line>();
	debugLineNormal->Initialize(camera->GetCamera());
	debugLineNormal->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f }); // 緑色を固定セット

	debugLineHit = std::make_unique<Line>();
	debugLineHit->Initialize(camera->GetCamera());
	debugLineHit->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // 赤色を固定セット

	// トレイルエフェクト
	trailEffect = std::make_shared<TrailEffect>();
	trailEffect->Initialize("Resource/trail/trail.png", trailTransform, width, trailMaxLifeTime);

}

void GamePlayScene::Update() {
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
		RailCameraGroupData& railSettings = GetOrCreateRailCameraGroup(*level->GetLevelData(), activeRailGroupName_);
		railCamera->SetDirectionMode(railSettings.directionMode);
		railCamera->SetTargetPosition(railSettings.targetPosition);
		railCamera->points.clear(); // 一旦リセット

		for (size_t index = 0; index < levelObjects.size(); ++index) {
			if (index >= level->GetLevelData()->objects.size() ||
				level->GetLevelData()->objects[index].groupName != activeRailGroupName_) continue;
			const auto& levelObject = levelObjects[index];
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
		if (railCamera->GetStartRevision() != lastRailStartRevision_) {
			ResetEnemySpawning();
			lastRailStartRevision_ = railCamera->GetStartRevision();
		}
		const bool isRailActive = railCamera->IsRailActive();

		// レール開始時はデバッグカメラよりレールカメラを優先する。
		if (isRailActive) {
			isDebugCamera = false;
		// Stop Rail Camera で停止した瞬間は、操作可能なデバッグカメラへ戻す。
		} else if (wasRailCameraActive) {
			isDebugCamera = true;
		}
		wasRailCameraActive = isRailActive;
		railCamera->Update();
	}

	// 2. レールカメラからベースとなる「座標」と「回転」を取得
	Vector3 basePos = railCamera->GetBasePosition();
	Vector3 baseRot = railCamera->GetBaseRotation();

	// 3. プレイヤーの入力による傾きなどのローカル更新
	player->SetTranslate(basePos);
	player->SetRotate(baseRot);



	// ロックオン処理の為に敵のポインタをまとめて渡す
	std::vector<Enemy*> enemyTargets;
	enemyTargets.reserve(enemies.size() + levelObjects.size());
	for (const auto& enemyObject : enemies) {
		if (auto* enemy = enemyObject->GetComponent<Enemy>()) {
				enemyTargets.push_back(enemy);
		}
	}
	for (const auto& levelObject : levelObjects) {
		if (levelObject->IsActive()) {
			if (auto* enemy = levelObject->GetComponent<Enemy>()) {
				enemyTargets.push_back(enemy);
			}
		}
	}
	player->SetDeltaTime(deltaTime);
	playerObject->Update();

	// デバックカメラ処理
	#ifdef USE_IMGUI
	if (isDebugCamera) {
		const Vector2 mousePosition = input->GetMouseScreen();
		const bool isMouseInGameView = ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Game") &&
			mousePosition.x >= gameViewPosition.x && mousePosition.x < gameViewPosition.x + gameViewSize.x &&
			mousePosition.y >= gameViewPosition.y && mousePosition.y < gameViewPosition.y + gameViewSize.y;
		// ImGui の捕捉状態ではなく、Game ビューの中にカーソルがあるかで判定する。
		if (isMouseInGameView) {
			camera->DebugCameraUpdate();
		}
	} else
	#endif
	{
		camera->SetTranslate(player->GetTranslate());
		camera->SetRotate(player->GetRotate());
		camera->Update();
	}
	// DebugCameraUpdate で変更された Transform も含め、コンポーネントを更新する。
	cameraObject->Update();

	// *スポナーの距離判定と敵の更新* //
	
	// エディタカメラ操作中ではない（ゲームプレイ中）場合のみ起動・生成を進める
	#ifdef USE_IMGUI
	if (!isDebugCamera) {
	#endif
		// レールカメラ上のプレイヤー座標（またはカメラ座標）を基準にする
		Vector3 targetPos = player->GetTranslate();

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
	#ifdef USE_IMGUI
	}
	#endif

	// カメラの現在の進行度を取得
	float cameraProgress = railCamera->GetRailT();

	// シーンに存在するすべての敵の更新処理
	for (auto& enemyObject : enemies) {
		if (auto* enemy = enemyObject->GetComponent<Enemy>()) {
			enemy->SetUpdateContext(player->GetTranslate(), cameraProgress);
			enemyObject->Update();
		}
	}

	// レベルオブジェクト
	for (auto& object : levelObjects) {
		if (auto* railMovement = object->GetComponent<ObjectRailMovementComponent>()) {
			railMovement->SetDeltaTime(deltaTime);
		}
		if (auto* easing = object->GetComponent<EasingComponent>()) {
			easing->SetDeltaTime(deltaTime);
		}
		if (object.get() == playerObject) {
			continue; // 操作対象プレイヤーは上で一度だけ更新する。
		}
		if (object.get() == cameraObject) {
			continue; // 通常カメラは入力反映直後に一度だけ更新する。
		}
		if (auto* enemy = object->GetComponent<Enemy>()) {
			enemy->SetUpdateContext(player->GetTranslate(), cameraProgress);
		}
		object->Update();
	}

	// プレイヤー・カメラ・敵の座標が全て確定した後にロックオンを更新する。
	// 描画に使うカメラ行列と同じ行列で照準を投影するため、レール移動中もずれない。
	player->UpdateLockOn(enemyTargets);
	player->UpdateReticle();

	// コックピット
	auto* cockpit = GameObject::FindByName("cockpit");
	if (cockpit) {
		auto* cockpitModel = cockpit ? cockpit->GetComponent<TransformComponent>() : nullptr;
		cockpitModel->SetTranslate(player->GetTranslate());
		cockpitModel->SetRotate(player->GetRotate());
	}

	// 当たり判定
	LevelEditorCommon::UpdateEnemyCollisions(*player, enemies, levelObjects);

	// --- 撃破された敵の削除 ---
	for (auto it = enemies.begin(); it != enemies.end();) {
		auto* enemy = (*it)->GetComponent<Enemy>();
		if (enemy && enemy->IsDead()) {
			// 削除(erase)する直前に、プレイヤーと弾へ通知してポインタを外させる
			player->RemoveBulletTarget(enemy);

			it = enemies.erase(it); // 実際の削除処理
		} else {
			++it;
		}
	}
	for (auto& levelObject : levelObjects) {
		if (auto* enemy = levelObject->GetComponent<Enemy>(); enemy && enemy->IsDead()) {
			player->RemoveBulletTarget(enemy);
			levelObject->SetActive(false);
		}
	}

	// *当たり判定の線* //
	// 毎フレーム描画前に前フレームの線データをクリア
	debugLineNormal->Clear();
	debugLineHit->Clear();
#ifdef _DEBUG
	// プレイヤーの当たり判定を登録
	if (player->IsHit()) {
		DrawOBB(debugLineHit.get(), LevelEditorCommon::GetColliderOBB(playerObject, player->GetOBB())); // 赤用のバッファに追加
	} else {
		DrawOBB(debugLineNormal.get(), LevelEditorCommon::GetColliderOBB(playerObject, player->GetOBB())); // 緑用のバッファに追加
	}

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

	// 弾の当たり判定
	for (const auto& bullet : player->GetBullets()) {
		DrawOBB(debugLineNormal.get(), bullet->GetOBB());
	}

	for (const auto& enemyObject : enemies) {
		auto* enemy = enemyObject->GetComponent<Enemy>();
		if (!enemy) continue;
		for (const auto& bullet : enemy->GetBullets()) {
			DrawOBB(debugLineNormal.get(), bullet->GetOBB());
		}
	}

#endif

	// スタート演出

	// HUDの演出
	if (isStartAction && !isFadeIn) {
		auto* reticle_1 = GameObject::FindByName("reticle_1");
		auto* reticle_2 = GameObject::FindByName("reticle_2");
		auto* reticle_3 = GameObject::FindByName("reticle_3");
		auto* monitor_1 = GameObject::FindByName("monitor_1");
		auto* monitorAction_1 = GameObject::FindByName("monitorAction_1");

		auto* reticle_1Easing = reticle_1->GetComponent<EasingComponent>();
		auto* reticle_2Easing = reticle_2->GetComponent<EasingComponent>();
		auto* reticle_3Easing = reticle_3->GetComponent<EasingComponent>();
		auto* monitorActionEasing = monitorAction_1->GetComponent<EasingComponent>();

		reticle_1->SetActive(true);
		if (reticle_1Easing && !reticle_1Easing->IsPlaying()) {
			reticle_2->SetActive(true);
		}
		if (reticle_2Easing && !reticle_2Easing->IsPlaying()) {
			reticle_3->SetActive(true);
		}
		if (reticle_3Easing && !reticle_3Easing->IsPlaying()) {
			monitor_1->SetActive(false);
			monitorAction_1->SetActive(true);
		}
		if (monitorActionEasing && !monitorActionEasing->IsPlaying() && !railCamera->IsRailActive()) {
			railCamera->StartRail();
			isRadialBlur = true;
		}

		const std::string& groupName = level->GetLevelData()->activeRailCameraGroup;

		if (groupName == "StartRailCamera") {
			if (railCamera->IsRailActive()) {
				// 放射線ブラー
				blurWidth = (std::max)(0.0f, blurWidth + 0.002f / 60.0f);
			}

			if (railCamera->GetRailT() >= 3.5f) {
				// フェードアウト
				intensity = (std::max)(0.0f, intensity - 1.0f / 30.0f);
				PostEffect::GetInstance()->SetIntensity(intensity);

				if (intensity <= 0.0f) {
					// そのままステージに切り換え
					activeRailGroupName_ = "StageRailCamera";
					level->GetLevelData()->activeRailCameraGroup = activeRailGroupName_;
					railCamera->StopRail();

					railCamera->StartRail();
					player->SetMoveEnabled(true); // プレイヤーの移動を有効化
					isStartAction = false;
					isFadeIn = true;
					isRadialBlur = false;

					// プレイ会用にセレクトシーンに飛ばす
					SceneManager::GetInstance()->ChangeScene("SELECT");

					// HUDの演出を非表示にする
					reticle_1->SetActive(false);
					reticle_2->SetActive(false);
					reticle_3->SetActive(false);
					monitorAction_1->SetActive(false);
				}
			}
		}
	}

	// フェードイン
	if (isFadeIn) {
		if (intensity < 1.0f) {
			intensity += 1.0f / 30.0f;
			PostEffect::GetInstance()->SetIntensity(intensity);
		} else {
			isFadeIn = false;
		}
	}


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
	// 太陽のライティング
	lightManager->SetUseSunLight(isSunLight);

	lightManager->Update();
	lightManager->UpdateSunLight();
#pragma endregion

#pragma region ポストエフェクト
	// *ポストエフェクト* //
	PostEffect::GetInstance()->Update(camera->GetCamera());

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
	PostEffect::GetInstance()->HightFogUpdate(camera->GetCamera());
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
	RayMarching::GetInstance()->Update(camera->GetCamera());
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
	const SceneEditorLayout editorLayout = ImGuiFunction::GetInstance()->BeginSceneEditor(
		*level, levelObjects, "gamePlayScene", gameViewPosition, gameViewSize, isGameViewHovered);
	ImGuiIO& editorIO = ImGui::GetIO();
	const float editorTopBarHeight = editorLayout.topBarHeight;
	const float editorLeftPaneWidth = editorLayout.leftPaneWidth;
	const float editorRightPaneWidth = editorLayout.rightPaneWidth;
	const float editorBottomPaneHeight = editorLayout.bottomPaneHeight;
	const float hierarchyHeight = editorLayout.hierarchyHeight;
	const ImVec2 gameViewWindowPosition(editorLayout.gameWindowPosition.x, editorLayout.gameWindowPosition.y);
	const ImVec2 gameViewWindowSize(editorLayout.gameWindowSize.x, editorLayout.gameWindowSize.y);

	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Settings")) {
		ImGui::SetNextWindowPos(ImVec2(editorLeftPaneWidth, editorIO.DisplaySize.y - editorBottomPaneHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(gameViewWindowSize.x, editorBottomPaneHeight), ImGuiCond_FirstUseEver);
		ImGui::Begin("Settings");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Settings");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Settings");

	// フレームレートの取得と表示
	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", fps > 0.0f ? 1000.0f / fps : 0.0f, fps);

	Transform& cameraTransform = cameraObject->GetTransform()->transform;
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

	// --- レールプリセットウィンドウ ---
	if (ImGuiFunction::GetInstance()->ShouldDrawDockableWindow("Rail Presets")) {
		ImGui::Begin("Rail Presets");
		ImGuiFunction::GetInstance()->TrackDockableWindow("Rail Presets");
		ImGuiFunction::GetInstance()->DrawMergedWindowTabs("Rail Presets");

		static char newPresetName[128] = "";
		ImGui::InputTextWithHint("##NewPreset", "Preset Name", newPresetName, IM_ARRAYSIZE(newPresetName));
		ImGui::SameLine();
		if (ImGui::Button("Save Active Group as Preset") && newPresetName[0] != '\0') {
			CreateRailPresetFromGroup(activeRailGroupName_, newPresetName);
			newPresetName[0] = '\0';
			level->SaveRailPresetsJson();
		}

		if (ImGui::Button("Reload Presets")) {
			level->LoadRailPresetsJson();
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Track Editor");

		if (!activeRailGroupName_.empty()) {
			RailCameraGroupData& activeGroupSettings = GetOrCreateRailCameraGroup(*level->GetLevelData(), activeRailGroupName_);
			bool sequenceChanged = false;

			ImGui::Text("Sequence for [%s]", activeRailGroupName_.c_str());
			ImGui::Indent();

			// 配置済みプリセットのリスト表示・置き換え・並び替え
			for (size_t i = 0; i < activeGroupSettings.presetSequence.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));

				auto& item = activeGroupSettings.presetSequence[i];

				// コンボボックスで別のプリセットに置き換え
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::BeginCombo("##ChangePreset", item.presetName.c_str())) {
					for (const auto& preset : level->GetRailPresets()) {
						bool isSelected = (item.presetName == preset.presetName);
						if (ImGui::Selectable(preset.presetName.c_str(), isSelected)) {
							item.presetName = preset.presetName;
							sequenceChanged = true;
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				// Y軸回転（Yaw）の調整ドラッグバーを追加
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				if (ImGui::DragFloat("Yaw", &item.localRotate.y, 1.0f, -360.0f, 360.0f, "%.0f deg")) {
					sequenceChanged = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Up") && i > 0) {
					std::swap(activeGroupSettings.presetSequence[i], activeGroupSettings.presetSequence[i - 1]);
					sequenceChanged = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Down") && i < activeGroupSettings.presetSequence.size() - 1) {
					std::swap(activeGroupSettings.presetSequence[i], activeGroupSettings.presetSequence[i + 1]);
					sequenceChanged = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete")) {
					activeGroupSettings.presetSequence.erase(activeGroupSettings.presetSequence.begin() + i);
					sequenceChanged = true;
				}

				ImGui::PopID();

				if (sequenceChanged) break; // リスト操作後は安全のためループを抜ける
			}
			ImGui::Unindent();

			ImGui::Spacing();
			ImGui::Text("Append Preset:");
			for (const auto& preset : level->GetRailPresets()) {
				if (ImGui::Button(("+ " + preset.presetName).c_str())) {
					// 文字列の代わりに TrackSequenceItem を作成して追加する
					TrackSequenceItem newItem;
					newItem.presetName = preset.presetName;
					newItem.localRotate = { 0.0f, 0.0f, 0.0f };

					activeGroupSettings.presetSequence.push_back(newItem);
					sequenceChanged = true;
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();

			// シーケンスに変更があった場合、自動的にレールを再構築する
			if (sequenceChanged) {
				RebuildRailGroupFromSequence(activeRailGroupName_);
				railCamera->StopRail(); // 再構築されたのでプレビューを停止
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Preview Controls");

			// 再生ボタン
			if (ImGui::Button("Play Rail Camera")) {
				if (!activeRailGroupName_.empty()) {
					// 1. アクティブなグループのポイントデータを収集する
					std::vector<ObjectData*> groupPoints;
					for (auto& objData : level->GetLevelData()->objects) {
						if (objData.groupName == activeRailGroupName_ &&
							std::find(objData.components.begin(), objData.components.end(), "RailPoint") != objData.components.end()) {
							groupPoints.push_back(&objData);
						}
					}

					// 2. Catmull-Rom 補間には最低4点必要なためチェック
					if (groupPoints.size() >= 4) {
						// 既存のカメラポイントをリセット
						railCamera->ClearPoints();

						// 収集したポイントの座標と回転をカメラにセット
						for (auto* p : groupPoints) {
							railCamera->AddPoint(p->transform.translate, p->transform.rotate);
						}

						// 再生開始
						railCamera->StartRail();
					} else {
						// ポイントが4つ未満の場合は警告を出す
						OutputDebugStringA("RailCamera requires at least 4 points to start!\n");
					}
				}
			}

			ImGui::SameLine();

			// 停止ボタン
			if (ImGui::Button("Stop")) {
				railCamera->StopRail();
			}

		} else {
			ImGui::TextDisabled("Select an active Rail Camera Group to use Track Editor.");
		}
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
	ImGui::SameLine();
	static char newGroupName[128]{};
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputTextWithHint("##NewGroup", "New group name", newGroupName, IM_ARRAYSIZE(newGroupName));
	ImGui::SameLine();
	if (ImGui::Button("Create Group") && newGroupName[0] != '\0') {
		const std::string groupName = newGroupName;
		auto& groups = level->GetLevelData()->groups;
		if (std::find(groups.begin(), groups.end(), groupName) == groups.end()) {
			groups.push_back(groupName);
		}
		newGroupName[0] = '\0';
	}

	ImGui::Separator();
	std::string groupToDelete;
	int objectToDuplicate = -1;
	int objectToDelete = -1;
	bool playerIsInHierarchy = false;
	for (const auto& object : levelObjects) {
		if (object.get() == playerObject) {
			playerIsInHierarchy = true;
			break;
		}
	}
	if (playerObject && playerObject->IsActive() && !playerIsInHierarchy) {
		const std::string playerLabel = std::string("[P] ") + playerObject->GetName() + "##RuntimePlayer";
		if (ImGui::Selectable(playerLabel.c_str(), selectedObject == playerObject)) {
			selectedObject = playerObject;
		}
		ImGui::Separator();
	}
	auto drawHierarchyObject = [&](size_t index) {
		auto* object = levelObjects[index].get();
		const bool isEmpty = !object->GetComponent<ModelRendererComponent>() &&
			!object->GetComponent<SpriteRendererComponent>() &&
			!object->GetComponent<TextRendererComponent>() &&
			!object->GetComponent<RailPointComponent>() &&
			!object->GetComponent<EnemySpawnerComponent>() &&
			!object->GetComponent<Player>() &&
			!object->GetComponent<Enemy>();
		const char* icon = object->GetComponent<Player>() ? "[P]" : (isEmpty ? "[E]" : "[O]");
		const std::string label = std::string(icon) + " " + object->GetName() + "##Hierarchy" + std::to_string(index);
		if (ImGui::Selectable(label.c_str(), selectedObject == object)) {
			selectedObject = object;
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("DND_OBJECT_TO_GROUP", &index, sizeof(index));
			ImGui::Text("Move %s", object->GetName().c_str());
			ImGui::EndDragDropSource();
		}
		const bool isPlayerObject = object->GetComponent<Player>() != nullptr;
		// 右クリックでオブジェクト操作メニューを開く。
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Duplicate Object", nullptr, false, !isPlayerObject)) {
				objectToDuplicate = static_cast<int>(index);
			}
			if (ImGui::MenuItem("Delete Object", nullptr, false, !isPlayerObject)) {
				objectToDelete = static_cast<int>(index);
			}
			if (isPlayerObject) ImGui::TextDisabled("The active player cannot be duplicated or deleted here.");
			ImGui::EndPopup();
		}
	};
	for (const std::string& groupName : level->GetLevelData()->groups) {
		const bool groupOpen = ImGui::TreeNodeEx(
			groupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_OBJECT_TO_GROUP")) {
				const size_t index = *static_cast<const size_t*>(payload->Data);
				if (index < level->GetLevelData()->objects.size()) {
					level->GetLevelData()->objects[index].groupName = groupName;
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete Group")) {
				groupToDelete = groupName;
			}
			ImGui::EndPopup();
		}
		if (groupOpen) {
			ImGui::PushID(groupName.c_str());
			const bool isActiveRailGroup = activeRailGroupName_ == groupName;
			if (ImGui::Button(isActiveRailGroup ? "Active Rail Camera" : "Use as Rail Camera")) {
				activeRailGroupName_ = groupName;
				level->GetLevelData()->activeRailCameraGroup = groupName;
				railCamera->StopRail();
			}
			if (isActiveRailGroup) {
				RailCameraGroupData& railSettings = GetOrCreateRailCameraGroup(*level->GetLevelData(), groupName);
				int directionMode = static_cast<int>(railSettings.directionMode);
				const char* directionModes[] = { "Follow Path", "Look At Target", "Use Point Rotation" };
				if (ImGui::Combo("Camera Direction", &directionMode, directionModes, IM_ARRAYSIZE(directionModes))) {
					railSettings.directionMode = static_cast<RailCameraDirectionMode>(directionMode);
				}
				if (railSettings.directionMode == RailCameraDirectionMode::LookAtTarget) {
					ImGui::DragFloat3("Look At Target", &railSettings.targetPosition.x, 0.1f);
				}
				ImGui::TextDisabled("Only Rail Camera Point objects in this group are used.");
			}
			ImGui::PopID();
			for (size_t index = 0; index < levelObjects.size(); ++index) {
				if (level->GetLevelData()->objects[index].groupName == groupName) {
					drawHierarchyObject(index);
				}
			}
			ImGui::TreePop();
		}
	}
	if (ImGui::TreeNodeEx("Ungrouped", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("UngroupedRailCamera");
		const bool isActiveRailGroup = activeRailGroupName_.empty();
		if (ImGui::Button(isActiveRailGroup ? "Active Rail Camera" : "Use as Rail Camera")) {
			activeRailGroupName_.clear();
			level->GetLevelData()->activeRailCameraGroup.clear();
			railCamera->StopRail();
		}
		if (isActiveRailGroup) {
			RailCameraGroupData& railSettings = GetOrCreateRailCameraGroup(*level->GetLevelData(), "");
			int directionMode = static_cast<int>(railSettings.directionMode);
			const char* directionModes[] = { "Follow Path", "Look At Target", "Use Point Rotation" };
			if (ImGui::Combo("Camera Direction", &directionMode, directionModes, IM_ARRAYSIZE(directionModes))) {
				railSettings.directionMode = static_cast<RailCameraDirectionMode>(directionMode);
			}
			if (railSettings.directionMode == RailCameraDirectionMode::LookAtTarget) {
				ImGui::DragFloat3("Look At Target", &railSettings.targetPosition.x, 0.1f);
			}
		}
		ImGui::PopID();
		for (size_t index = 0; index < levelObjects.size(); ++index) {
			if (level->GetLevelData()->objects[index].groupName.empty()) {
				drawHierarchyObject(index);
			}
		}
		ImGui::TreePop();
	}
	if (!groupToDelete.empty()) {
		if (activeRailGroupName_ == groupToDelete) {
			activeRailGroupName_.clear();
			level->GetLevelData()->activeRailCameraGroup.clear();
			railCamera->StopRail();
		}
		for (auto& objectData : level->GetLevelData()->objects) {
			if (objectData.groupName == groupToDelete) objectData.groupName.clear();
		}
		auto& groups = level->GetLevelData()->groups;
		groups.erase(std::remove(groups.begin(), groups.end(), groupToDelete), groups.end());
		auto& railGroups = level->GetLevelData()->railCameraGroups;
		railGroups.erase(std::remove_if(railGroups.begin(), railGroups.end(),
			[&groupToDelete](const RailCameraGroupData& group) { return group.groupName == groupToDelete; }), railGroups.end());
	}
	if (objectToDuplicate >= 0) {
		ObjectData copyData = level->GetLevelData()->objects[objectToDuplicate];
		const std::string sourceName = copyData.name;
		copyData.name = sourceName + " Copy";
		int suffix = 2;
		while (std::any_of(level->GetLevelData()->objects.begin(), level->GetLevelData()->objects.end(),
			[&copyData](const ObjectData& objectData) { return objectData.name == copyData.name; })) {
			copyData.name = sourceName + " Copy " + std::to_string(suffix++);
		}
		copyData.transform.translate.x += 1.0f;
		auto copyObject = std::make_unique<GameObject>(copyData.name);
		copyObject->GetTransform()->transform = copyData.transform;
		const auto* sourceObject = levelObjects[objectToDuplicate].get();
		if (sourceObject->GetComponent<ModelRendererComponent>()) {
			auto* renderer = copyObject->AddComponent<ModelRendererComponent>();
			LevelEditorCommon::ConfigureBuildingLod(*renderer, copyData);
			if (!renderer->HasLod()) {
				const bool isRail = copyData.type == "RAIL" || copyData.type == "rail";
				const bool isSpawner = copyData.type == "SPAWNER" || copyData.type == "spawner";
				renderer->SetModel(isRail ? "rail.obj" : (isSpawner ? "cube.gltf" : (copyData.file_name.empty() ? "cube.gltf" : copyData.file_name)));
			}
			renderer->SetMaxDrawDistance(copyData.maxDrawDistance);
			renderer->SetOutlineEnabled(copyData.modelOutlineEnabled);
			renderer->SetOutlineThickness(copyData.modelOutlineThickness);
			renderer->SetOutlineColor(copyData.modelOutlineColor);
			renderer->SetColorOverrideEnabled(copyData.modelColorOverrideEnabled);
			renderer->SetColorOverride(copyData.modelColorOverride);
			renderer->SetColorOverrideUnlit(copyData.modelColorOverrideUnlit);
			renderer->SetToonShadingEnabled(copyData.modelToonShading);
			renderer->SetTextureOverride(copyData.modelTextureOverride);
		}
		if (sourceObject->GetComponent<RectTransformComponent>()) {
			auto* rectTransform = copyObject->AddComponent<RectTransformComponent>();
			rectTransform->position = copyData.rectPosition;
			rectTransform->rotation = copyData.rectRotation;
			rectTransform->scale = copyData.rectScale;
		}
		if (sourceObject->GetComponent<ColliderComponent>()) {
			auto* collider = copyObject->AddComponent<ColliderComponent>();
			collider->size = copyData.colliderSize;
			collider->centerOffset = copyData.colliderCenterOffset;
		}
		if (sourceObject->GetComponent<SpriteRendererComponent>()) {
			if (!copyObject->GetComponent<RectTransformComponent>()) copyObject->AddComponent<RectTransformComponent>();
			auto* spriteRenderer = copyObject->AddComponent<SpriteRendererComponent>();
			spriteRenderer->SetTexture(copyData.sprite_file_name.empty() ? "Resource/title/title.png" : copyData.sprite_file_name);
			spriteRenderer->SetSize(copyData.spriteSize);
			spriteRenderer->SetEmissive(copyData.spriteEmissiveColor, copyData.spriteEmissiveIntensity);
		}
		if (sourceObject->GetComponent<TextRendererComponent>()) {
			if (!copyObject->GetComponent<RectTransformComponent>()) copyObject->AddComponent<RectTransformComponent>();
			auto* textRenderer = copyObject->AddComponent<TextRendererComponent>();
			textRenderer->SetText(copyData.text);
			textRenderer->SetFontFamilyUtf8(copyData.textFontFamily);
			textRenderer->SetFontSize(copyData.textFontSize);
			textRenderer->SetColor(copyData.textColor);
			textRenderer->SetMaxWidth(copyData.textMaxWidth);
			textRenderer->SetBold(copyData.textBold);
			textRenderer->SetOutlineEnabled(copyData.textOutlineEnabled);
			textRenderer->SetOutlineThickness(copyData.textOutlineThickness);
			textRenderer->SetOutlineColor(copyData.textOutlineColor);
			textRenderer->SetEmissive(copyData.textEmissiveColor, copyData.textEmissiveIntensity);
			textRenderer->SetCharacterSpacing(copyData.textCharacterSpacing);
			textRenderer->SetMeshOffsets(copyData.textMeshOffsets);
		}
		if (sourceObject->GetComponent<RailPointComponent>()) copyObject->AddComponent<RailPointComponent>();
		if (sourceObject->GetComponent<ObjectRailMovementComponent>()) {
			auto* railMovement = copyObject->AddComponent<ObjectRailMovementComponent>();
			railMovement->Configure(copyData.objectRailPoints, copyData.objectRailSpeed,
				copyData.objectRailLoop, copyData.objectRailOrientToPath, copyData.objectRailPlayOnStart,
				copyData.objectRailPointRotations, copyData.objectRailUsePointRotations);
		}
		if (sourceObject->GetComponent<EasingComponent>()) {
			copyObject->AddComponent<EasingComponent>()->Configure(copyData.easing);
		}
		if (sourceObject->GetComponent<EnemySpawnerComponent>()) {
			auto* spawner = copyObject->AddComponent<EnemySpawnerComponent>();
			spawner->Configure(copyData.spawnDataList, spawnDistance, railCamera.get());
		}
		if (sourceObject->GetComponent<Enemy>()) {
			auto* enemy = copyObject->AddComponent<EnemyNormal>();
			enemy->SetTransform(copyData.transform);
		}
		copyObject->SetActive(copyData.active);
		copyObject->Initialize();
		selectedObject = copyObject.get();
		levelObjects.push_back(std::move(copyObject));
		level->GetLevelData()->objects.push_back(std::move(copyData));
	}
	if (objectToDelete >= 0 && levelObjects[objectToDelete].get() == cameraObject) {
		OutputDebugStringA("The active camera Empty cannot be deleted.\n");
		objectToDelete = -1;
	}
	if (objectToDelete >= 0) {
		if (selectedObject == levelObjects[objectToDelete].get()) {
			selectedObject = nullptr;
		}
		levelObjects.erase(levelObjects.begin() + objectToDelete);
		level->GetLevelData()->objects.erase(level->GetLevelData()->objects.begin() + objectToDelete);
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
	ImGui::Text("Textures (drag to a Sprite or Model Texture Override field):");
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
	LevelEditorCommon::DrawCityGenerator(*level, levelObjects, selectedObject);
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
				Vector3 spawnPos = camera->GetTranslate();
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
				level->GetLevelData()->objects[selectedIndex].active = isActive;
			}

			ImGui::Text("Type: %s", objType.c_str());
			if (!objFile.empty()) {
				ImGui::Text("File: %s", objFile.c_str());
			}
		} else {
			// Runtime Player など、レベルデータと紐づかないオブジェクト用の表示。
			ImGui::Text("Type: %s", selectedObject == playerObject ? "Player" : "Runtime GameObject");
		}

		ImGui::Separator();

		// Transform は必須コンポーネント。Unity の Inspector と同じように常に表示する。
		auto transformComp = selectedObject->GetTransform();
		if (transformComp && ImGui::CollapsingHeader(selectedObject->GetComponent<RectTransformComponent>() ? "Transform (3D - unused by Sprite)" : "Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool transformChanged = ImGui::DragFloat3("Position", &transformComp->transform.translate.x, 0.1f);
			transformChanged |= ImGui::DragFloat3("Rotation", &transformComp->transform.rotate.x, 0.05f);
			transformChanged |= ImGui::DragFloat3("Scale", &transformComp->transform.scale.x, 0.1f);
			if (transformChanged && selectedIndex != -1) {
				level->GetLevelData()->objects[selectedIndex].transform = transformComp->transform;
			}
		}

		if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size() &&
			ImGui::CollapsingHeader("Group", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto& objectData = level->GetLevelData()->objects[selectedIndex];
			const char* groupPreview = objectData.groupName.empty() ? "<Ungrouped>" : objectData.groupName.c_str();
			if (ImGui::BeginCombo("Group", groupPreview)) {
				if (ImGui::Selectable("<Ungrouped>", objectData.groupName.empty())) {
					objectData.groupName.clear();
				}
				for (const std::string& groupName : level->GetLevelData()->groups) {
					if (ImGui::Selectable(groupName.c_str(), objectData.groupName == groupName)) {
						objectData.groupName = groupName;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::TextDisabled("Groups organize the editor only; transforms are unchanged.");
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
				bool outlineEnabled = renderer->IsOutlineEnabled();
				if (ImGui::Checkbox("Outline OnOff##ModelRenderer", &outlineEnabled)) {
					renderer->SetOutlineEnabled(outlineEnabled);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].modelOutlineEnabled = outlineEnabled;
					}
				}
				ImGui::BeginDisabled(!outlineEnabled);
				float outlineThickness = renderer->GetOutlineThickness();
				if (ImGui::DragFloat("Outline Thickness##ModelRenderer", &outlineThickness, 0.001f, 0.0f, 1.0f)) {
					renderer->SetOutlineThickness(outlineThickness);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].modelOutlineThickness = renderer->GetOutlineThickness();
					}
				}
				Vector4 outlineColor = renderer->GetOutlineColor();
				if (ImGui::ColorEdit4("Outline Color##ModelRenderer", &outlineColor.x)) {
					renderer->SetOutlineColor(outlineColor);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].modelOutlineColor = outlineColor;
					}
				}
				ImGui::EndDisabled();
				bool colorOverrideEnabled = renderer->IsColorOverrideEnabled();
				if (ImGui::Checkbox("Color Override##ModelRenderer", &colorOverrideEnabled)) {
					renderer->SetColorOverrideEnabled(colorOverrideEnabled);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].modelColorOverrideEnabled = colorOverrideEnabled;
				}
				ImGui::BeginDisabled(!colorOverrideEnabled);
				Vector4 colorOverride = renderer->GetColorOverride();
				if (ImGui::ColorEdit4("Override Color##ModelRenderer", &colorOverride.x)) {
					renderer->SetColorOverride(colorOverride);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].modelColorOverride = colorOverride;
				}
				bool colorOverrideUnlit = renderer->IsColorOverrideUnlit();
				if (ImGui::Checkbox("Flat / Silhouette##ModelRenderer", &colorOverrideUnlit)) {
					renderer->SetColorOverrideUnlit(colorOverrideUnlit);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].modelColorOverrideUnlit = colorOverrideUnlit;
				}
				ImGui::EndDisabled();
				int lightingStyle = renderer->IsToonShadingEnabled() ? 0 : 1;
				if (ImGui::Combo("Lighting Style##ModelRenderer", &lightingStyle, "Toon\0Realistic\0")) {
					renderer->SetToonShadingEnabled(lightingStyle == 0);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].modelToonShading = renderer->IsToonShadingEnabled();
					}
				}

				char modelPath[260]{};
				strncpy_s(modelPath, renderer->GetModelPath().c_str(), _TRUNCATE);
				if (ImGui::InputText("Model", modelPath, IM_ARRAYSIZE(modelPath))) {
					renderer->SetModel(modelPath);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						auto& objectData = level->GetLevelData()->objects[selectedIndex];
						objectData.file_name = modelPath;
						objectData.lodHighModel.clear();
						objectData.lodMediumModel.clear();
						objectData.lodLowModel.clear();
					}
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_FILE")) {
						const std::string droppedModel = static_cast<const char*>(payload->Data);
						renderer->SetModel(droppedModel);
						if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
							auto& objectData = level->GetLevelData()->objects[selectedIndex];
							objectData.file_name = droppedModel;
							objectData.lodHighModel.clear();
							objectData.lodMediumModel.clear();
							objectData.lodLowModel.clear();
						}
					}
					ImGui::EndDragDropTarget();
				}
				char textureOverridePath[260]{};
				strncpy_s(textureOverridePath, renderer->GetTextureOverridePath().c_str(), _TRUNCATE);
				if (ImGui::InputText("Texture Override##ModelRenderer", textureOverridePath, IM_ARRAYSIZE(textureOverridePath))) {
					renderer->SetTextureOverride(textureOverridePath);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].modelTextureOverride = textureOverridePath;
					}
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TEXTURE_FILE")) {
						const std::string droppedTexture = static_cast<const char*>(payload->Data);
						renderer->SetTextureOverride(droppedTexture);
						if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
							level->GetLevelData()->objects[selectedIndex].modelTextureOverride = droppedTexture;
						}
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::TextDisabled("Leave empty to use the model's original texture.");
				float maxDrawDistance = renderer->GetMaxDrawDistance();
				if (ImGui::DragFloat("Max Draw Distance", &maxDrawDistance, 1.0f, 0.0f, 10000.0f)) {
					renderer->SetMaxDrawDistance(maxDrawDistance);
					if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
						level->GetLevelData()->objects[selectedIndex].maxDrawDistance = maxDrawDistance;
					}
				}
				if (selectedIndex != -1 && selectedIndex < level->GetLevelData()->objects.size()) {
					LevelEditorCommon::DrawBuildingLodInspector(*renderer, level->GetLevelData()->objects[selectedIndex]);
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
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].spriteSize = spriteSize;
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
				ImGui::Separator();
				ImGui::TextUnformatted("Mesh Deformation 4x4 (pixels)");
				ImGui::TextDisabled("Rows: top to bottom. Columns: left to right.");
				auto meshOffsets = textRenderer->GetMeshOffsets();
				bool meshChanged = false;
				for (size_t row = 0; row < TextRendererComponent::kMeshRowCount; ++row) {
					ImGui::Text("Row %d", static_cast<int>(row + 1));
					ImGui::Indent();
					for (size_t column = 0; column < TextRendererComponent::kMeshColumnCount; ++column) {
						ImGui::PushID(static_cast<int>(row * TextRendererComponent::kMeshColumnCount + column));
						meshChanged |= ImGui::DragFloat2("Point", &meshOffsets[row * TextRendererComponent::kMeshColumnCount + column].x,
							0.1f, -500.0f, 500.0f);
						ImGui::PopID();
					}
					ImGui::Unindent();
				}
				if (ImGui::Button("Reset Mesh Deformation")) {
					meshOffsets = {};
					meshChanged = true;
				}
				if (meshChanged) {
					textRenderer->SetMeshOffsets(meshOffsets);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].textMeshOffsets = meshOffsets;
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

		if (auto* easing = selectedObject->GetComponent<EasingComponent>()) {
			if (ImGui::CollapsingHeader("Easing", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (DrawEasingComponentInspector(*easing) && selectedIndex != -1) {
					level->GetLevelData()->objects[selectedIndex].easing = easing->GetSettings();
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
		if (auto* railMovement = selectedObject->GetComponent<ObjectRailMovementComponent>()) {
			if (ImGui::CollapsingHeader("Object Rail Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool enabled = railMovement->IsEnabled();
				if (ImGui::Checkbox("Enabled##ObjectRailMovement", &enabled)) railMovement->SetEnabled(enabled);
				bool playing = railMovement->IsPlaying();
				if (ImGui::Checkbox("Playing##ObjectRailMovement", &playing)) railMovement->SetPlaying(playing);
				ImGui::SameLine();
				if (ImGui::Button("Restart##ObjectRailMovement")) railMovement->Restart();
				float speed = railMovement->GetSpeed();
				if (ImGui::DragFloat("Speed (segments/sec)##ObjectRailMovement", &speed, 0.01f, 0.0f, 100.0f)) {
					railMovement->SetSpeed(speed);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].objectRailSpeed = railMovement->GetSpeed();
				}
				bool loop = railMovement->IsLooping();
				if (ImGui::Checkbox("Loop##ObjectRailMovement", &loop)) {
					railMovement->SetLooping(loop);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].objectRailLoop = loop;
				}
				bool orient = railMovement->IsOrientToPath();
				if (ImGui::Checkbox("Orient To Path##ObjectRailMovement", &orient)) {
					railMovement->SetOrientToPath(orient);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].objectRailOrientToPath = orient;
				}
				bool usePointRotations = railMovement->UsesPointRotations();
				if (ImGui::Checkbox("Use Point Rotations##ObjectRailMovement", &usePointRotations)) {
					railMovement->SetUsePointRotations(usePointRotations);
					if (selectedIndex != -1) level->GetLevelData()->objects[selectedIndex].objectRailUsePointRotations = usePointRotations;
				}
				auto& points = railMovement->GetPoints();
				auto& rotations = railMovement->GetPointRotations();
				ImGui::Text("Control Points: %d", static_cast<int>(points.size()));
				for (size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
					ImGui::PushID(static_cast<int>(pointIndex));
					if (ImGui::DragFloat3("Point", &points[pointIndex].x, 0.1f) && selectedIndex != -1) {
						level->GetLevelData()->objects[selectedIndex].objectRailPoints = points;
					}
					if (ImGui::DragFloat3("Rotation", &rotations[pointIndex].x, 0.01f) && selectedIndex != -1) {
						level->GetLevelData()->objects[selectedIndex].objectRailPointRotations = rotations;
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Remove") && points.size() > 2) {
						points.erase(points.begin() + pointIndex);
						rotations.erase(rotations.begin() + pointIndex);
						if (selectedIndex != -1) {
							level->GetLevelData()->objects[selectedIndex].objectRailPoints = points;
							level->GetLevelData()->objects[selectedIndex].objectRailPointRotations = rotations;
						}
						ImGui::PopID();
						break;
					}
					ImGui::PopID();
				}
				if (ImGui::Button("Add Point At Object")) {
					points.push_back(selectedObject->GetTransform()->GetTranslate());
					rotations.push_back(selectedObject->GetTransform()->GetRotate());
					if (selectedIndex != -1) {
						level->GetLevelData()->objects[selectedIndex].objectRailPoints = points;
						level->GetLevelData()->objects[selectedIndex].objectRailPointRotations = rotations;
					}
				}
			}
		}
		if (auto* cameraComponent = selectedObject->GetComponent<CameraComponent>()) {
			if (ImGui::CollapsingHeader("Camera")) {
				bool enabled = cameraComponent->IsEnabled();
				if (ImGui::Checkbox("Enabled##Camera", &enabled)) {
					cameraComponent->SetEnabled(enabled);
				}
				if (ImGui::Button("Set as Main Camera")) {
					camera = cameraComponent;
					cameraObject = selectedObject;
					CameraManager::GetInstance()->AddCamera("main", camera->GetCamera());
					CameraManager::GetInstance()->SetActiveCamera("main");
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
				objectData.spriteSize = spriteRenderer->GetSize();
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
			if (!selectedObject->GetComponent<ObjectRailMovementComponent>() && ImGui::Button("Object Rail Movement")) {
				auto* railMovement = selectedObject->AddComponent<ObjectRailMovementComponent>();
				const Vector3 start = selectedObject->GetTransform()->GetTranslate();
				const std::vector<Vector3> points = { start, start + Vector3{ 0.0f, 0.0f, 5.0f } };
				const Vector3 rotation = selectedObject->GetTransform()->GetRotate();
				const std::vector<Vector3> rotations = { rotation, rotation };
				railMovement->Configure(points, 1.0f, true, true, true, rotations, false);
				objectData.objectRailPoints = points;
				objectData.objectRailPointRotations = rotations;
				objectData.objectRailSpeed = 1.0f;
				objectData.objectRailLoop = true;
				objectData.objectRailOrientToPath = true;
				objectData.objectRailUsePointRotations = false;
				objectData.objectRailPlayOnStart = true;
				addSerializedComponent("ObjectRailMovement");
			}
			if (!selectedObject->GetComponent<EasingComponent>() && ImGui::Button("Easing")) {
				EasingData easingData{};
				if (auto* rectTransform = selectedObject->GetComponent<RectTransformComponent>()) {
					easingData.startPosition = easingData.goalPosition = { rectTransform->position.x, rectTransform->position.y, 0.0f };
					easingData.startRotation = easingData.goalRotation = { 0.0f, 0.0f, rectTransform->rotation };
					easingData.startScale = easingData.goalScale = { rectTransform->scale.x, rectTransform->scale.y, 1.0f };
				} else {
					const Transform& transform = selectedObject->GetTransform()->transform;
					easingData.startPosition = easingData.goalPosition = transform.translate;
					easingData.startRotation = easingData.goalRotation = transform.rotate;
					easingData.startScale = easingData.goalScale = transform.scale;
				}
				if (auto* textRenderer = selectedObject->GetComponent<TextRendererComponent>()) {
					easingData.startColor = easingData.goalColor = textRenderer->GetColor();
				} else if (auto* spriteRenderer = selectedObject->GetComponent<SpriteRendererComponent>()) {
					easingData.startColor = easingData.goalColor = spriteRenderer->GetColor();
				} else if (auto* modelRenderer = selectedObject->GetComponent<ModelRendererComponent>(); modelRenderer && modelRenderer->IsColorOverrideEnabled()) {
					easingData.startColor = easingData.goalColor = modelRenderer->GetColorOverride();
				}
				objectData.easing = easingData;
				selectedObject->AddComponent<EasingComponent>()->Configure(objectData.easing);
				addSerializedComponent("Easing");
			}
			if ((objectData.type == "EMPTY" || objectData.type == "empty") &&
				!selectedObject->GetComponent<CameraComponent>() && ImGui::Button("Camera")) {
				selectedObject->AddComponent<CameraComponent>();
				addSerializedComponent("Camera");
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
			if (!selectedObject->GetComponent<Player>() && ImGui::Button("Player Controller")) {
				auto* playerComponent = selectedObject->AddComponent<Player>();
				// 追加した Player をゲームの操作対象として使用する。
				defaultPlayerObject.reset();
				playerObject = selectedObject;
				player = playerComponent;
				addSerializedComponent("Player");
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

		if ((selectedIndex != -1 || selectedObject == playerObject) && ImGui::Button("Delete Object", ImVec2(-1, 0))) {
			if (selectedObject == playerObject) {
				// Keep a hidden fallback alive so gameplay code never retains a dangling Player pointer.
				defaultPlayerObject = std::make_unique<GameObject>("Deleted Player Fallback");
				playerObject = defaultPlayerObject.get();
				player = playerObject->AddComponent<Player>();
				playerObject->SetActive(false);
				playerObject->Initialize();
			}
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

void GamePlayScene::Draw2D() {
	// 2Dオブジェクトの描画準備
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	// プレイヤーレティクル
	if (!isStartAction) {
		playerObject->Draw();
	}

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
void GamePlayScene::Draw3D() {
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

	// アウトラインを有効化したレベルオブジェクトのみを描画する。
	for (const auto& object : levelObjects) {
		if (auto* renderer = object->GetComponent<ModelRendererComponent>()) {
			renderer->DrawOutline();
		}
	}

}

void GamePlayScene::Finalize() {
	CameraManager::GetInstance()->RemoveCamera("main");
}

void GamePlayScene::ResetEnemySpawning() {
	for (const auto& levelObject : levelObjects) {
		if (auto* spawner = levelObject->GetComponent<EnemySpawnerComponent>()) {
			spawner->ResetSpawnState();
		}
	}

	// 既存の敵を破棄する前に、プレイヤーが保持するロックオン参照を解除する。
	for (const auto& enemyObject : enemies) {
		if (player) {
			player->RemoveBulletTarget(enemyObject->GetComponent<Enemy>());
		}
	}
	enemies.clear();
}

void GamePlayScene::CreateRailPresetFromGroup(const std::string& groupName, const std::string& presetName) {
#ifdef USE_IMGUI
	RailPresetData preset;
	preset.presetName = presetName;

	// グループに属するレールポイントを抽出
	std::vector<Transform> groupTransforms;
	for (size_t i = 0; i < levelObjects.size(); ++i) {
		if (level->GetLevelData()->objects[i].groupName == groupName) {
			if (levelObjects[i]->GetComponent<RailPointComponent>()) {
				groupTransforms.push_back(levelObjects[i]->GetTransform()->transform);
			}
		}
	}

	if (groupTransforms.empty()) return;

	// 始点のワールド行列とその逆行列を計算
	Matrix4x4 startMat = MakeRotateMatrix(groupTransforms[0].rotate) * MakeTranslateMatrix(groupTransforms[0].translate);
	Matrix4x4 invStartMat = Inverse(startMat);

	// 始点を基準としたローカル座標に変換して保存
	for (const auto& worldTransform : groupTransforms) {
		Matrix4x4 worldMat = MakeRotateMatrix(worldTransform.rotate) * MakeTranslateMatrix(worldTransform.translate);
		Matrix4x4 localMat = worldMat * invStartMat;

		Transform localTransform;
		// ※ localMatから translate と rotate を抽出する処理（ImGuizmo::DecomposeMatrixToComponents 等を使用）
		float translation[3], rotation[3], scale[3];
		ImGuizmo::DecomposeMatrixToComponents(&localMat.m[0][0], translation, rotation, scale);

		localTransform.translate = { translation[0], translation[1], translation[2] };
		localTransform.rotate = {
			rotation[0] * (3.14159265f / 180.0f),
			rotation[1] * (3.14159265f / 180.0f),
			rotation[2] * (3.14159265f / 180.0f)
		};
		localTransform.scale = { 1.0f, 1.0f, 1.0f };
		preset.localTransforms.push_back(localTransform);
	}

	level->GetRailPresets().push_back(preset);

#endif
}

void GamePlayScene::AppendRailPreset(const RailPresetData& preset) {
#ifdef USE_IMGUI
	if (preset.localTransforms.empty()) return;

	Transform baseTransform;
	baseTransform.translate = { 0,0,0 };
	baseTransform.rotate = { 0,0,0 };
	baseTransform.scale = { 1,1,1 };

	// アクティブなレールグループの終端ポイントを検索
	for (auto it = levelObjects.rbegin(); it != levelObjects.rend(); ++it) {
		auto* obj = it->get();
		// インデックス取得用（LevelDataとの同期のため）
		int64_t idx = std::distance(levelObjects.begin(), it.base()) - 1;
		if (level->GetLevelData()->objects[idx].groupName == activeRailGroupName_ &&
			obj->GetComponent<RailPointComponent>()) {
			baseTransform = obj->GetTransform()->transform;
			break;
		}
	}

	Matrix4x4 baseMat = MakeRotateMatrix(baseTransform.rotate) * MakeTranslateMatrix(baseTransform.translate);

	// プリセットの始点は baseTransform と重なるため、インデックス1から生成する
	for (size_t i = 1; i < preset.localTransforms.size(); ++i) {
		Matrix4x4 localMat = MakeRotateMatrix(preset.localTransforms[i].rotate) * MakeTranslateMatrix(preset.localTransforms[i].translate);
		Matrix4x4 newWorldMat = localMat * baseMat;

		float translation[3], rotation[3], scale[3];
		ImGuizmo::DecomposeMatrixToComponents(&newWorldMat.m[0][0], translation, rotation, scale);

		// 新しいGameObjectの生成（GamePlaySceneの「Add Rail Point」相当の処理）
		const std::string objectName = "RailPoint_" + preset.presetName + "_" + std::to_string(levelObjects.size() + 1);
		auto newObject = std::make_unique<GameObject>(objectName);
		auto transformComp = newObject->AddComponent<TransformComponent>();

		transformComp->transform.translate = { translation[0], translation[1], translation[2] };
		transformComp->transform.rotate = {
			rotation[0] * (3.14159265f / 180.0f),
			rotation[1] * (3.14159265f / 180.0f),
			rotation[2] * (3.14159265f / 180.0f)
		};
		transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

		auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
		modelRenderer->SetModel("rail.obj");
		newObject->AddComponent<RailPointComponent>();
		newObject->Initialize();

		// LevelDataへの登録
		ObjectData newObjectData;
		newObjectData.type = "RAIL";
		newObjectData.name = objectName;
		newObjectData.file_name = "rail.obj";
		newObjectData.groupName = activeRailGroupName_;
		newObjectData.transform = transformComp->transform;
		newObjectData.components = { "ModelRenderer", "RailPoint" };

		level->GetLevelData()->objects.push_back(newObjectData);
		levelObjects.push_back(std::move(newObject));
	}
#endif

}

void GamePlayScene::RebuildRailGroupFromSequence(const std::string& groupName) {
#ifdef USE_IMGUI	
if (groupName.empty()) return;

	// 1. 指定グループの RailPoint をすべて収集し、インデックスを記録[cite: 10]
	std::vector<int> groupPointIndices;
	for (size_t i = 0; i < level->GetLevelData()->objects.size(); ++i) { //[cite: 10]
		auto& objData = level->GetLevelData()->objects[i]; //[cite: 10]
		if (objData.groupName == groupName &&
			std::find(objData.components.begin(), objData.components.end(), "RailPoint") != objData.components.end()) { //[cite: 10]
			groupPointIndices.push_back(static_cast<int>(i)); //[cite: 10]
		}
	}

	if (groupPointIndices.empty()) return; // 基準となる開始ポイントがない場合は何もしない[cite: 10]

	// 2. 最初のポイント(基準点)の Transform を保存[cite: 10]
	Transform baseStartTransform = level->GetLevelData()->objects[groupPointIndices[0]].transform; //[cite: 10]

	// 3. 基準点(0番目) 以外の古いポイントを後ろから削除（インデックスずれ防止）[cite: 10]
	for (int i = static_cast<int>(groupPointIndices.size()) - 1; i >= 1; --i) { //[cite: 10]
		int deleteIdx = groupPointIndices[i]; //[cite: 10]
		if (selectedObject == levelObjects[deleteIdx].get()) { //[cite: 10]
			selectedObject = nullptr; // 削除対象を選択中なら解除[cite: 10]
		}
		level->GetLevelData()->objects.erase(level->GetLevelData()->objects.begin() + deleteIdx); //[cite: 10]
		levelObjects.erase(levelObjects.begin() + deleteIdx); //[cite: 10]
	}

	// 4. シーケンスに従ってプリセットを再生成
	RailCameraGroupData& railSettings = GetOrCreateRailCameraGroup(*level->GetLevelData(), groupName);
	Transform currentEndTransform = baseStartTransform;

	constexpr float deg2rad = 3.14159265f / 180.0f;

	// 一番最初のプリセットかどうかを判定するフラグ
	bool isFirstPreset = true;

	for (const auto& item : railSettings.presetSequence) {
		const RailPresetData* targetPreset = nullptr;
		for (const auto& p : level->GetRailPresets()) {
			if (p.presetName == item.presetName) {
				targetPreset = &p;
				break;
			}
		}
		if (!targetPreset || targetPreset->localTransforms.empty()) continue;

		// 追加回転角（度数法→ラジアン）を加算した基準姿勢を作成
		Vector3 sectionRotate = {
			currentEndTransform.rotate.x + item.localRotate.x * deg2rad,
			currentEndTransform.rotate.y + item.localRotate.y * deg2rad,
			currentEndTransform.rotate.z + item.localRotate.z * deg2rad
		};

		// ★追加: 一番最初のプリセットの場合、グループの基準点（0番目の制御点）の回転もこれに合わせて上書きする
		if (isFirstPreset) {
			level->GetLevelData()->objects[groupPointIndices[0]].transform.rotate = sectionRotate;
			if (auto* transformComp = levelObjects[groupPointIndices[0]]->GetComponent<TransformComponent>()) {
				transformComp->transform.rotate = sectionRotate;
			}
			isFirstPreset = false;
		}

		Matrix4x4 baseMat = MakeRotateMatrix(sectionRotate) * MakeTranslateMatrix(currentEndTransform.translate);

		// インデックス1以降のポイントをワールド座標に変換して追加
		for (size_t i = 1; i < targetPreset->localTransforms.size(); ++i) {
			Matrix4x4 localMat = MakeRotateMatrix(targetPreset->localTransforms[i].rotate) * MakeTranslateMatrix(targetPreset->localTransforms[i].translate);
			Matrix4x4 newWorldMat = localMat * baseMat;

			float translation[3], rotation[3], scale[3];
			ImGuizmo::DecomposeMatrixToComponents(&newWorldMat.m[0][0], translation, rotation, scale);

			Transform newTrans;
			newTrans.translate = { translation[0], translation[1], translation[2] };
			newTrans.rotate = {
				rotation[0] * deg2rad,
				rotation[1] * deg2rad,
				rotation[2] * deg2rad
			};
			newTrans.scale = { 1.0f, 1.0f, 1.0f };

			// 新規 GameObject の生成
			const std::string objectName = "RailPoint_" + targetPreset->presetName + "_" + std::to_string(levelObjects.size() + 1);
			auto newObject = std::make_unique<GameObject>(objectName);
			auto transformComp = newObject->AddComponent<TransformComponent>();
			transformComp->transform = newTrans;

			auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel("rail.obj");
			newObject->AddComponent<RailPointComponent>();
			newObject->Initialize();

			// LevelData に登録
			ObjectData newObjectData;
			newObjectData.type = "RAIL";
			newObjectData.name = objectName;
			newObjectData.file_name = "rail.obj";
			newObjectData.groupName = groupName;
			newObjectData.transform = newTrans;
			newObjectData.components = { "ModelRenderer", "RailPoint" };

			level->GetLevelData()->objects.push_back(newObjectData);
			levelObjects.push_back(std::move(newObject));

			// 繋いだ最後のポイントを、次のプリセットの基準点として更新
			currentEndTransform = newTrans;
		}
	}
#endif
}

void GamePlayScene::CreateLevel() {
	auto configureEasing = [](GameObject& gameObject, const ObjectData& objectData) {
		if (std::find(objectData.components.begin(), objectData.components.end(), "Easing") != objectData.components.end()) {
			gameObject.AddComponent<EasingComponent>()->Configure(objectData.easing);
		}
	};

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
			LevelEditorCommon::ConfigureBuildingLod(*modelRenderer, objectData);
			if (!modelRenderer->HasLod()) {
				modelRenderer->SetModel(objectData.file_name);
			}
			modelRenderer->SetMaxDrawDistance(objectData.maxDrawDistance);
			modelRenderer->SetOutlineEnabled(objectData.modelOutlineEnabled);
			modelRenderer->SetOutlineThickness(objectData.modelOutlineThickness);
			modelRenderer->SetOutlineColor(objectData.modelOutlineColor);
			modelRenderer->SetColorOverrideEnabled(objectData.modelColorOverrideEnabled);
			modelRenderer->SetColorOverride(objectData.modelColorOverride);
			modelRenderer->SetColorOverrideUnlit(objectData.modelColorOverrideUnlit);
			modelRenderer->SetToonShadingEnabled(objectData.modelToonShading);
			modelRenderer->SetTextureOverride(objectData.modelTextureOverride);
			if (std::find(objectData.components.begin(), objectData.components.end(), "ObjectRailMovement") != objectData.components.end()) {
				auto* railMovement = gameObject->AddComponent<ObjectRailMovementComponent>();
				railMovement->Configure(objectData.objectRailPoints, objectData.objectRailSpeed,
					objectData.objectRailLoop, objectData.objectRailOrientToPath, objectData.objectRailPlayOnStart,
					objectData.objectRailPointRotations, objectData.objectRailUsePointRotations);
			}

			// 4. アタッチされたコンポーネントを一括初期化
			configureEasing(*gameObject, objectData);
			gameObject->SetActive(objectData.active);
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
			modelRenderer->SetOutlineEnabled(objectData.modelOutlineEnabled);
			modelRenderer->SetOutlineThickness(objectData.modelOutlineThickness);
			modelRenderer->SetOutlineColor(objectData.modelOutlineColor);
			modelRenderer->SetColorOverrideEnabled(objectData.modelColorOverrideEnabled);
			modelRenderer->SetColorOverride(objectData.modelColorOverride);
			modelRenderer->SetColorOverrideUnlit(objectData.modelColorOverrideUnlit);
			modelRenderer->SetToonShadingEnabled(objectData.modelToonShading);
			modelRenderer->SetTextureOverride(objectData.modelTextureOverride);
			gameObject->AddComponent<RailPointComponent>();
			if (std::find(objectData.components.begin(), objectData.components.end(), "ObjectRailMovement") != objectData.components.end()) {
				auto* railMovement = gameObject->AddComponent<ObjectRailMovementComponent>();
				railMovement->Configure(objectData.objectRailPoints, objectData.objectRailSpeed,
					objectData.objectRailLoop, objectData.objectRailOrientToPath, objectData.objectRailPlayOnStart,
					objectData.objectRailPointRotations, objectData.objectRailUsePointRotations);
			}

			configureEasing(*gameObject, objectData);
			gameObject->SetActive(objectData.active);
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
			modelRenderer->SetOutlineEnabled(objectData.modelOutlineEnabled);
			modelRenderer->SetOutlineThickness(objectData.modelOutlineThickness);
			modelRenderer->SetOutlineColor(objectData.modelOutlineColor);
			modelRenderer->SetColorOverrideEnabled(objectData.modelColorOverrideEnabled);
			modelRenderer->SetColorOverride(objectData.modelColorOverride);
			modelRenderer->SetColorOverrideUnlit(objectData.modelColorOverrideUnlit);
			modelRenderer->SetToonShadingEnabled(objectData.modelToonShading);
			modelRenderer->SetTextureOverride(objectData.modelTextureOverride);

			// ゲームロジック用のスポナーも同じ GameObject にアタッチする。
			auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
			spawner->Configure(objectData.spawnDataList, spawnDistance, railCamera.get());
			if (std::find(objectData.components.begin(), objectData.components.end(), "ObjectRailMovement") != objectData.components.end()) {
				auto* railMovement = gameObject->AddComponent<ObjectRailMovementComponent>();
				railMovement->Configure(objectData.objectRailPoints, objectData.objectRailSpeed,
					objectData.objectRailLoop, objectData.objectRailOrientToPath, objectData.objectRailPlayOnStart,
					objectData.objectRailPointRotations, objectData.objectRailUsePointRotations);
			}

			configureEasing(*gameObject, objectData);
			gameObject->SetActive(objectData.active);
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
				LevelEditorCommon::ConfigureBuildingLod(*renderer, objectData);
				if (!renderer->HasLod()) {
					renderer->SetModel(objectData.file_name.empty() ? "cube.gltf" : objectData.file_name);
				}
				renderer->SetMaxDrawDistance(objectData.maxDrawDistance);
				renderer->SetOutlineEnabled(objectData.modelOutlineEnabled);
				renderer->SetOutlineThickness(objectData.modelOutlineThickness);
				renderer->SetOutlineColor(objectData.modelOutlineColor);
				renderer->SetColorOverrideEnabled(objectData.modelColorOverrideEnabled);
				renderer->SetColorOverride(objectData.modelColorOverride);
				renderer->SetColorOverrideUnlit(objectData.modelColorOverrideUnlit);
				renderer->SetToonShadingEnabled(objectData.modelToonShading);
				renderer->SetTextureOverride(objectData.modelTextureOverride);
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
				spriteRenderer->SetSize(objectData.spriteSize);
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
				textRenderer->SetMeshOffsets(objectData.textMeshOffsets);
			}
			if (hasComponent("RailPoint")) {
				gameObject->AddComponent<RailPointComponent>();
			}
			if (hasComponent("ObjectRailMovement")) {
				auto* railMovement = gameObject->AddComponent<ObjectRailMovementComponent>();
				railMovement->Configure(objectData.objectRailPoints, objectData.objectRailSpeed,
					objectData.objectRailLoop, objectData.objectRailOrientToPath, objectData.objectRailPlayOnStart,
					objectData.objectRailPointRotations, objectData.objectRailUsePointRotations);
			}
			if (hasComponent("Camera")) {
				auto* cameraComponent = gameObject->AddComponent<CameraComponent>();
				// The first Camera component in the level is the default main camera.
				if (!camera) {
					camera = cameraComponent;
					cameraObject = gameObject.get();
				}
			}
			if (hasComponent("EnemySpawner")) {
				auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
				spawner->Configure(objectData.spawnDataList, spawnDistance, railCamera.get());
			}
			if (hasComponent("Player")) {
				player = gameObject->AddComponent<Player>();
				defaultPlayerObject.reset();
				playerObject = gameObject.get();
			}
			if (hasComponent("EnemyNormal")) {
				auto* enemy = gameObject->AddComponent<EnemyNormal>();
				enemy->SetTransform(objectData.transform);
			}
			configureEasing(*gameObject, objectData);
			gameObject->SetActive(objectData.active);
			gameObject->Initialize();
			levelObjects.push_back(std::move(gameObject));
		}

	}

}

#ifdef USE_IMGUI

void GamePlayScene::GizmoUpdate(bool showEditorControls) {
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
#ifdef TEXT_MESH_EDITOR_OVERLAY
	bool isPointerOverTextMeshVertex = false;
	if (selectedObject) {
		if (auto* textRenderer = selectedObject->GetComponent<TextRendererComponent>()) {
			if (auto* rectTransform = selectedObject->GetComponent<RectTransformComponent>()) {
				isPointerOverTextMeshVertex = TextMeshEditorGizmo::DrawAndSelect(*textRenderer, *rectTransform,
					gameViewPosition, gameViewSize, selectedTextMeshVertex_);
			} else {
				selectedTextMeshVertex_ = -1;
			}
		} else {
			selectedTextMeshVertex_ = -1;
		}
	}
#else
	constexpr bool isPointerOverTextMeshVertex = false;
#endif
	if (ImGui::IsMouseClicked(0) && isMouseInGameView && !ImGuizmo::IsOver() && !isPointerOverTextMeshVertex && !isEditorPanelHovered) {
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
			bool isEditingTextMesh = false;
#ifdef TEXT_MESH_EDITOR_OVERLAY
			if (isSpriteObject) {
				if (auto* textRenderer = selectedObject->GetComponent<TextRendererComponent>()) {
					isEditingTextMesh = selectedTextMeshVertex_ >= 0;
					if (isEditingTextMesh && TextMeshEditorGizmo::ManipulateSelected(*textRenderer, *rectTransform,
						viewMat, projMat, selectedTextMeshVertex_)) {
						for (size_t index = 0; index < levelObjects.size(); ++index) {
							if (levelObjects[index].get() == selectedObject) {
								level->GetLevelData()->objects[index].textMeshOffsets = textRenderer->GetMeshOffsets();
								break;
							}
						}
					}
				}
			}
#endif

			// 3. ギズモの操作と行列の更新
			if (!isEditingTextMesh) {
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

#else

void GamePlayScene::GizmoUpdate([[maybe_unused]] bool showEditorControls) {}

#endif

