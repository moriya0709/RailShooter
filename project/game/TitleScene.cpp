#include "TitleScene.h"
#include "ObjectCommon.h"
#include "SpriteCommon.h"
#include "LineCommon.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "TrailEffectManager.h"
#include "LightManager.h"
#include "TransformComponent.h"
#include "ModelRendererComponent.h"
#include "AnimatorComponent.h"

void TitleScene::Initialize() {

	// カメラ初期化
	camera = std::make_unique<Camera>();
	camera->SetRotate({ cameraTransform.rotate });
	camera->SetTranslate({ cameraTransform.translate });

	// カメラマネージャ登録
	CameraManager::GetInstance()->AddCamera("main", camera.get());
	CameraManager::GetInstance()->SetActiveCamera("main");

	// 3Dオブジェクト
	// アニメーションするキャラクターの生成例
	object = std::make_unique<GameObject>();

	// 1. 位置情報
	transform = object->AddComponent<TransformComponent>();

	// 2. 描画
	auto renderer = object->AddComponent<ModelRendererComponent>();
	renderer->SetModel("walk.gltf");

	// 3. アニメーション制御 (追加)
	animator = object->AddComponent<AnimatorComponent>();

	object->Initialize();

	// アニメーション再生
	animator->PlayAnimation("walk", 0.2f);

	// パーティクル

	particleMesh = std::make_unique<ParticleEmitter>();
	particleMesh->Initialize("obj", transformParticle, 5, 0.1f);
	particleMesh->SetActive("obj");
	particleMesh->LoadParticle("Resource/particle/test.csv");

	// トレイルエフェクト
	trailEffect->Initialize("Resource/trail/trail.png", transformParticle, 1.0f, 1.5f);
	trailEffect->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	trailEffect->LoadCsv("Resource/trail/shot.csv");
	trailEffect->SetDistance(0.01f);
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

	// 線
	line = std::make_unique<Line>();
	line->Initialize(camera.get());
	
	// 音声再生
	//SoundManager::GetInstance()->Play("bgm");

}

void TitleScene::Update() {
	// 入力取得
	auto input = Input::GetInstance();
	// カメラ更新
	CameraManager::GetInstance()->Update();

	// * 3Dオブジェクト* //
	object->Update();
	animator->BoneLineUpdate(line.get(), transform->GetScale(), transform->GetRotate(), transform->GetTranslate());

	//　線
	line->Update();

	// アニメーション変更
	if (input->TriggerKey(DIK_0)) {
		animator->PlayAnimation("walk", 1.0f);
	}
	if (input->TriggerKey(DIK_1)) {
		animator->PlayAnimation("sneakWalk", 1.0f);
	}

	Vector3 vector = { 0.0f,0.0f,0.0f };

	if (input->PushKey(DIK_W)) {
		objectTransform.rotate.y = 0.0f;
		vector.z = 0.05f;
	}
	if (input->PushKey(DIK_S)) {
		objectTransform.rotate.y = 3.14f;
		vector.z = -0.05f;
	}
	if (input->PushKey(DIK_A)) {
		objectTransform.rotate.y = 4.66f;
		vector.x = -0.05f;
	}
	if (input->PushKey(DIK_D)) {
		objectTransform.rotate.y = 1.52f;
		vector.x = 0.05f;
	}

	objectTransform.translate += vector;

	bool isWalk = false;
	if (vector.x != 0.0f || vector.z != 0.0f) {
		isWalk = true;
	}

	if (isWalk) {
		animator->PlayAnimation("sneakWalk", 0.3f);
	} else {
		animator->PlayAnimation("walk", 0.3f);
	}

	transform->SetTranslate(objectTransform.translate);
	transform->SetRotate(objectTransform.rotate);

	if (input->TriggerKey(DIK_SPACE)) {
		// ゲームプレイシーン(次シーン)を生成
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}

	// *パーティクル* //
	//if (isTornado) {
	//	for (int i = 0; i < kTornadoCount; i++) {
	//		tornado[i]->Update();
	//	}
	//}


	// 左手の座標
	transformParticle.translate = animator->GetJointPosition("mixamorig:LeftHand");
	particleMesh->SetTranslate(transformParticle.translate);

	particleMesh->Update();
	particleMesh->Editor();

	// マウスのワールド座標変換
	//transformParticle.translate = input->GetMouseWorld(camera.get());

	// トレイルエフェクト更新
	//trailEffect->AddPoint(transformParticle.translate);
	//trailEffect->SetTranslate(transformParticle.translate);
	
	// スカイボックス
	Skybox::GetInstance()->Update();

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
	//PostEffect::GetInstance()->Update(camera.get());

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

	ImGui::Checkbox("isTornado", &isTornado);

	ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.1f, -500.0f, 500.0f);
	ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f, -10.0f, 10.0f);
	camera->SetTranslate(cameraTransform.translate);
	camera->SetRotate(cameraTransform.rotate);

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
			ImGui::DragInt("lensFlareGhostCount", &lensFlareGhostCount, 1, 0,10);
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
	ImGui::DragFloat3("rayMarchingSunDir", &rayMarchingSunDir.x, 0.01f,-1.0f,1.0f);
	ImGui::DragFloat("rayMarchingCloudCoverage", &rayMarchingCloudCoverage, 0.01f,-5.0f,10.0f);
	ImGui::DragFloat("rayMarchingCloudBottom", &rayMarchingCloudBottom, 10.0f,-5000.0f,5000.0f);
	ImGui::DragFloat("rayMarchingCloudTop", &rayMarchingCloudTop, 10.0f, -5000.0f, 5000.0f);
	ImGui::Checkbox("rayMarchingIsRialLight", &rayMarchingIsRialLight);
	ImGui::Checkbox("rayMarchingIsAnimeLight", &rayMarchingIsAnimeLight);
	ImGui::Checkbox("rayMarchingIsMotionBlur", &rayMarchingIsMotionBlur);
	ImGui::DragFloat("rayMarchingCloudOpacity", &rayMarchingCloudOpacity, 0.001f, 0.0f, 0.1f);
	ImGui::Checkbox("isStorm", &isStorm);
	ImGui::DragFloat("thunderFrequency", &thunderFrequency, 0.001f, 0.0f, 10.0f);
	ImGui::DragFloat("thunderBrightness", &thunderBrightness, 0.01f, 0.0f, 300.0f);


#pragma endregion

#endif

}

void TitleScene::Draw2D() {
	// 2Dオブジェクトの描画準備
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	// スプライト描画
	//sprite->Draw();
}
void TitleScene::Draw3D() {
	// スカイボックス
	//Skybox::GetInstance()->Draw();

	// 3Dオブジェクトの描画準備
	//ObjectCommon::GetInstance()->SetCommonPipelineState();

	
	// 3Dオブジェクト描画
	//object->Draw();

	// アニメーション
	//ObjectCommon::GetInstance()->SetAnimationPipelineState();

	// 3Dオブジェクト描画
	object->Draw();

	// アウトライン描画準備
	ObjectCommon::GetInstance()->SetOutlinePipelineState();

	// 線描画準備
	LineCommon::GetInstance()->SetCommonPipelineState();
	
	line->Draw();
	line->Clear();
	
}

void TitleScene::Finalize() {
	CameraManager::GetInstance()->RemoveCamera("main");
}
