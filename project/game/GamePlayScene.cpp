#include "GamePlayScene.h"
#include "ObjectCommon.h"
#include "SpriteCommon.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "TransformComponent.h"
#include "ModelRendererComponent.h"
#include "AnimatorComponent.h"
#include "SkyBox.h"
#include <ModelManager.h>

void GamePlayScene::Initialize() {

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
	level->LoadJson("scene");
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

	// プレイヤー
	player = std::make_unique<Player>();
	player->Initialize();

}

void GamePlayScene::Update() {
	// 入力取得
	auto input = Input::GetInstance();
	// カメラ更新
	CameraManager::GetInstance()->Update();
	// 時間更新
	deltaTime = gameTimer->Tick();

	player->Update(deltaTime);



	// 1. レールカメラの更新（レール上の現在位置・回転を計算）
	// 2. ★ GameObject の座標を RailCamera の制御点として毎フレーム上書き（同期）する
	if (railCamera) {
		railCamera->points.clear(); // 一旦リセット

		for (size_t i = 0; i < levelObjects.size(); ++i) {
			// levelData と同期している前提でタイプをチェック
			if (i < level->GetLevelData()->objects.size()) {
				std::string objType = level->GetLevelData()->objects[i].type;

				// RAILタイプのオブジェクトを見つけたら、その座標を RailCamera に渡す
				if (objType == "RAIL" || objType == "rail") {
					auto transformComp = levelObjects[i]->GetComponent<TransformComponent>();
					if (transformComp) {
						RailPoint p{};
						p.position = transformComp->transform.translate;
						p.rotate = transformComp->transform.rotate;
						railCamera->points.push_back(p);
					}
				}
			}
		}
	}

	// 3. その後、RailCamera自身の更新処理を呼ぶ
	if (railCamera) {
		railCamera->EditorUpdate();
		railCamera->Update();
	}

	// 2. レールカメラからベースとなる「座標」と「回転」を取得
	Vector3 basePos = railCamera->GetBasePosition();
	Vector3 baseRot = railCamera->GetBaseRotation();

	// 3. プレイヤーの入力による傾きなどのローカル更新
	player->SetTranslate(basePos);
	player->SetRotate(baseRot);
	player->Update(deltaTime);


	if (isDebugCamera) {
		if (!ImGui::GetIO().WantCaptureMouse) {
			camera->DebugCameraUpdate();
		}
	} else {
		camera->SetTranslate(player->GetTranslate());
		camera->SetRotate(player->GetRotate());
		camera->Update();
	}

	// レベルオブジェクト
	for (auto& object : levelObjects) {
		object->Update();
	}

	// ENTERキーを押したら
	if (input->TriggerKey(DIK_RETURN)) {
		// ゲームプレイシーン(次シーン)を生成
		SceneManager::GetInstance()->ChangeScene("Title");
	}

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


	// スカイボックス
	//Skybox::GetInstance()->Update();

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


#pragma endregion

#ifdef USE_IMGUI
	// ImGui
	// フレームレートの取得と表示
	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / fps, fps);

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


#pragma endregion

	// Gizmo
	GizmoUpdate();

	// --- アセットブラウザ ウィンドウ ---
	ImGui::Begin("Asset Browser");

	// アセットとして追加したいモデルのファイルリスト（本来はフォルダ内を自動全検索してもOK）
	std::vector<std::string> modelFiles = ModelManager::GetInstance()->GetLoadedModelNames();

	ImGui::Text("Drag a model to the scene:");
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

	// レールの制御点
	if (ImGui::Button("Add Rail Point")) {
		// 1. 新しい GameObject を生成
		auto newObject = std::make_unique<GameObject>();

		// 2. TransformComponent の追加と配置設定
		auto transformComp = newObject->AddComponent<TransformComponent>();

		transformComp->transform.translate = camera->GetTranslate();
		transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

		// 3. ModelRendererComponent の追加（モデルは固定で "rail.obj" を指定）
		auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
		modelRenderer->SetModel("rail.obj");

		// 4. 初期化
		newObject->Initialize();

		// 5. 生成したオブジェクトを即座に選択状態にする（ギズモですぐ動かせるように）
		selectedObject = newObject.get();

		// 6. ゲームシーンの更新リストに追加
		levelObjects.push_back(std::move(newObject));

		// 7. JSON保存用の LevelData にも「RAIL」タイプとして登録
		ObjectData newObjectData;
		newObjectData.type = "RAIL"; // ★ ここを RAIL にする
		newObjectData.name = "RailPoint_" + std::to_string(levelObjects.size());
		newObjectData.file_name = "rail.obj";
		newObjectData.transform = transformComp->transform;

		level->GetLevelData()->objects.push_back(newObjectData);

		OutputDebugStringA("★★★ Added New Rail Point!\n");
	}
	// 敵のスポーンイベント地点
	if (ImGui::Button("Add Enemy Spawner")) {
		auto newObject = std::make_unique<GameObject>();
		auto transformComp = newObject->AddComponent<TransformComponent>();

		// カメラの少し前に配置するなど、出しやすい位置に設定
		transformComp->transform.translate = camera->GetTranslate();
		transformComp->transform.translate.z += 10.0f;
		transformComp->transform.rotate = { 0.0f, 0.0f, 0.0f };
		transformComp->transform.scale = { 1.0f, 1.0f, 1.0f };

		// エディタ上で視認するためのダミーモデル（例: "cube.obj"）をセット
		auto modelRenderer = newObject->AddComponent<ModelRendererComponent>();
		modelRenderer->SetModel("cube.gltf");

		newObject->Initialize();
		selectedObject = newObject.get(); // 生成してすぐ選択状態に
		levelObjects.push_back(std::move(newObject));

		// LevelData に「SPAWNER」として登録
		ObjectData newObjectData;
		newObjectData.type = "SPAWNER"; // ★ タイプを SPAWNER にする
		newObjectData.name = "Spawner_" + std::to_string(levelObjects.size());
		// file_name に「どの敵を出すか」の情報を間借りして保存するのもオススメです
		newObjectData.file_name = "EnemyTypeA";
		newObjectData.transform = transformComp->transform;

		level->GetLevelData()->objects.push_back(newObjectData);
	}

	ImGui::End();
	// ★ ドラッグ操作中（マウスで何かを掴んでいる時）だけドロップ処理を有効化する
	if (ImGui::GetDragDropPayload() != nullptr) {

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::Begin("ViewportDropTarget", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoBringToFrontOnFocus
		);

		// 画面全体を覆う見えないボタン（ドラッグ中のみ出現）
		ImGui::InvisibleButton("##ViewportDropArea", ImGui::GetIO().DisplaySize);

		// ドロップ判定
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_FILE")) {
				std::string droppedFileName = (const char*)payload->Data;

				// 1. 新しい GameObject を生成
				auto newObject = std::make_unique<GameObject>();

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
				newObjectData.name = "SpawnedObject_" + std::to_string(levelObjects.size());
				newObjectData.file_name = droppedFileName;
				newObjectData.transform = transformComp->transform;

				level->GetLevelData()->objects.push_back(newObjectData);

				OutputDebugStringA(("★★★ Spawned: " + droppedFileName + "\n").c_str());
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}

	// --- インスペクター ウィンドウ ---
	ImGui::Begin("Inspector");

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

			ImGui::Text("Name: %s", objName.c_str());
			ImGui::Text("Type: %s", objType.c_str());
			if (!objFile.empty()) {
				ImGui::Text("File: %s", objFile.c_str());
			}
		} else {
			// 万が一 LevelData と紐づいていない場合のフォールバック
			ImGui::Text("Type: Unknown GameObject");
		}

		ImGui::Separator();

		// Transform情報の取得と表示・編集
		auto transformComp = selectedObject->GetComponent<TransformComponent>();
		if (transformComp) {
			ImGui::Text("Transform");
			ImGui::DragFloat3("Position", &transformComp->transform.translate.x, 0.1f);
			ImGui::DragFloat3("Rotation", &transformComp->transform.rotate.x, 0.05f);
			ImGui::DragFloat3("Scale", &transformComp->transform.scale.x, 0.1f);
		}

		// オブジェクト削除ボタン（誤誤爆防止のために赤色スタイル適用）
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

		if (ImGui::Button("Delete Object", ImVec2(-1, 0))) { // -1指定で横幅いっぱいに拡大
			// 1. LevelData(セーブ用)配列から削除
			level->GetLevelData()->objects.erase(level->GetLevelData()->objects.begin() + selectedIndex);

			// 2. levelObjects(実体)配列から削除
			levelObjects.erase(levelObjects.begin() + selectedIndex);

			// 3. 選択状態を解除してNULLにする（ポインタ参照エラー・クラッシュ防止）
			selectedObject = nullptr;

			// スタイルを元に戻して、これ以降の描画処理を行わずに抜ける
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

			if (currentObjData.type == "SPAWNER" || currentObjData.type == "spawner") {
				ImGui::Separator();
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "◆ Spawner Settings");

				if (ImGui::Button("Add Enemy to Spawner")) {
					// 初期値として追加
					currentObjData.spawnDataList.push_back({ 0.0f, { 0.0f, 0.0f, 0.0f }, "NORMAL" });
				}

				ImGui::Spacing();

				// プルダウン（Combo）に表示する敵の種類のリスト
				const char* enemyTypes[] = { "NORMAL", "FAST", "BOSS" };
				int typeCount = IM_ARRAYSIZE(enemyTypes);

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
					for (int j = 0; j < typeCount; j++) {
						if (sData.type == enemyTypes[j]) {
							currentTypeIndex = j;
							break;
						}
					}

					// プルダウンのUIを表示し、変更があったら sData.type に文字列を書き戻す
					if (ImGui::Combo("Type", &currentTypeIndex, enemyTypes, typeCount)) {
						sData.type = enemyTypes[currentTypeIndex];
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

#endif

}

void GamePlayScene::Draw2D() {
	// 2Dオブジェクトの描画準備
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	// スプライト描画
	player->Draw();
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

void GamePlayScene::Finalize() {
	CameraManager::GetInstance()->RemoveCamera("main");
}

void GamePlayScene::CreateLevel() {
	for (auto& objectData : level->GetLevelData()->objects) {
		if (objectData.type == "MESH" || objectData.type == "mesh") {
			// 1. GameObject の生成
			auto gameObject = std::make_unique<GameObject>();

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
		} else if (objectData.type == "SPAWNER" || objectData.type == "spawner") {
			// SPAWNER の実体オブジェクトを生成して levelObjects に登録する
			auto gameObject = std::make_unique<GameObject>();

			auto transform = gameObject->AddComponent<TransformComponent>();
			transform->transform.translate = objectData.transform.translate;
			transform->transform.rotate = objectData.transform.rotate;
			transform->transform.scale = objectData.transform.scale;

			// エディタ表示用のモデルをアタッチ
			auto modelRenderer = gameObject->AddComponent<ModelRendererComponent>();
			modelRenderer->SetModel("cube.gltf");

			gameObject->Initialize();
			levelObjects.push_back(std::move(gameObject));
		}
		
	}
}

void GamePlayScene::GizmoUpdate() {
	auto input = Input::GetInstance();

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

	// マウス左クリックの瞬間 ＆ ImGuizmoを操作中でない場合のみ判定
	if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGui::GetIO().WantCaptureMouse) {
		Vector2 mousePos = input->GetMouseScreen(); // ※ご自身のInputクラスの関数に合わせる

		float windowWidth = 1920.0f; // 画面幅
		float windowHeight = 1080.0f; // 画面高さ

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
			// 1. ImGuizmoの初期設定
			ImGuizmo::SetOrthographic(false); // パースペクティブ(透視投影)カメラを使用

			// ★修正2: 小さなデフォルトウィンドウの制限を受けないよう、フルスクリーンの背景に描画・判定をセットする
			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());

			// 画面サイズの設定
			float windowWidth = 1920.0f;
			float windowHeight = 1080.0f;
			ImGuizmo::SetRect(0, 0, windowWidth, windowHeight);

			// ★追加：UI上にマウスがある、かつギズモをドラッグ中でない場合はギズモの操作を無効化する
			bool isUIHovered = ImGui::GetIO().WantCaptureMouse;
			bool isGizmoDragging = ImGuizmo::IsUsing();
			ImGuizmo::Enable(!isUIHovered || isGizmoDragging);

			// 2. カメラの行列を取得
			Matrix4x4 viewMat = camera->GetViewMatrix();
			Matrix4x4 projMat = camera->GetProjectionMatrix();
			Matrix4x4 worldMat = transformComp->GetWorldMatrix();

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

				// ★修正：操作モードによって、全体の行列(worldMat)を使うか、差分行列(deltaMat)を使うか分ける
				if (currentGizmoOperation == ImGuizmo::TRANSLATE) {
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

	// セーブ機能のUI
	ImGui::Separator(); // 区切り線
	if (ImGui::Button("Save JSON Level")) {
		// 1. (既存の処理) 画面上の各 GameObject (MESH) の最新 Transform を LevelData に同期させる
		for (size_t i = 0; i < levelObjects.size(); ++i) {
			if (i < level->GetLevelData()->objects.size()) {
				auto transformComp = levelObjects[i]->GetComponent<TransformComponent>();
				if (transformComp) {
					level->GetLevelData()->objects[i].transform = transformComp->transform;
				}
			}
		}

		// 2. ★ LevelData から古い RAIL データをすべて削除する（std::remove_if を使用）
		auto& objects = level->GetLevelData()->objects;
		objects.erase(
			std::remove_if(objects.begin(), objects.end(), [](const ObjectData& obj) {
				return obj.type == "RAIL" || obj.type == "rail";
				}),
			objects.end()
		);

		// 3. ★ RailCamera の現在の制御点を新しい RAIL データとして LevelData に追加
		for (size_t i = 0; i < railCamera->points.size(); ++i) {
			ObjectData railObj;
			railObj.type = "RAIL";
			railObj.name = "RailPoint_" + std::to_string(i);
			railObj.transform.translate = railCamera->points[i].position;
			railObj.transform.rotate = railCamera->points[i].rotate;
			railObj.transform.scale = { 1.0f, 1.0f, 1.0f }; // 制御点のスケールはダミー値
			objects.push_back(railObj);
		}

		// 4. JSON へ書き出し
		level->SaveJson("scene");
	}
}
