#include "RailCamera.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "LineCommon.h"

void RailCamera::Initialize() {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	windowAPI_ = std::make_unique <WindowAPI>();

	// カメラ初期化
	camera = std::make_unique <Camera>();
	camera->SetRotate({ cameraTransform.rotate });
	camera->SetTranslate({ cameraTransform.translate });

	// カメラマネージャ登録
	CameraManager::GetInstance()->AddCamera("main", camera.get());
	CameraManager::GetInstance()->SetActiveCamera("main");

	// レールライン初期化
	railLine = std::make_unique<Line>();
	railLine->Initialize(camera.get());

}

void RailCamera::Update() {
	// カメラ更新
	CameraManager::GetInstance()->Update();
	// Catmull-Rom 補間には 4 点必要。Empty を制御点にした場合も、
	// 4 点未満ではプレビュー線だけを表示し、無効な添字アクセスはしない。
	if (isRail && points.size() >= 4) {
		const float tEnd = static_cast<float>(points.size() - 3);

		// tを進める
		railT += railSpeed * deltaTime;

		// 終端クランプ
		if (railT >= tEnd) {
			railT = tEnd - 0.001f;
		}

		// カメラ位置を更新
		cameraTransform.translate = Evaluate(railT);
		camera->SetTranslate(cameraTransform.translate);

		// カメラ回転を更新
		if (cameraMode == CameraMode::Direction) {
			UpdateCameraDirection();
		} else if (cameraMode == CameraMode::Target) {
			UpdateCameraTarget();
		} else if (cameraMode == CameraMode::Specified) {
			UpdateCameraSpecified();
		}

	}
}

void RailCamera::StartRail() {
	if (!CanStartRail()) {
		return;
	}

	railT = 0.0f;
	isRail = true;
}

void RailCamera::EditorUpdate() { 
#ifdef USE_IMGUI 
	if (ImGui::Begin("RailEditor")) {
		if (ImGui::Button("Add Point")) {
			AddPoint(camera->GetTranslate(), camera->GetRotate());
		}
		ImGui::Text("PointCount : %d", (int)points.size());

		// 選択中の点の情報表示
		if (selectedPoint >= 0 && selectedPoint < (int)points.size()) {
			ImGui::Separator();
			ImGui::Text("Selected: Point[%d]", selectedPoint);
			ImGui::DragFloat3("Position", &points[selectedPoint].position.x, 0.1f);
			ImGui::DragFloat3("Rotate", &points[selectedPoint].rotate.x, 0.01f);
			if (ImGui::Button("Delete Point")) {
				points.erase(points.begin() + selectedPoint);
				spheres.erase(spheres.begin() + selectedPoint);
				selectedPoint = -1;
			}
		}

		ImGui::DragFloat("time", &railT,0.01f);
		const bool canStart = CanStartRail();
		if (!canStart) {
			ImGui::TextDisabled("Add %d more Rail Camera Point(s) to start.", 4 - static_cast<int>(points.size()));
		}
		ImGui::BeginDisabled(!canStart);
		if (ImGui::Button("Start / Restart Rail Camera")) {
			StartRail();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Stop Rail Camera")) {
			StopRail();
		}
		// カメラ
		ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f, -180.0f, 180.0f);
		camera->SetTranslate({ cameraTransform.translate });
		camera->SetRotate({ cameraTransform.rotate });

	}

	ImGui::End();

	// ギズモ描画（ImGuizmoを使う場合）
	UpdateGizmo();
#endif
}

void RailCamera::EditorDraw() {
#ifdef USE_IMGUI 
	if (!isRail) {
		// クリックによる点の選択
		if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver()) {
			SelectPointByMouse();
		}

		// sphere描画（選択中は色を変える）
		size_t count = std::min(points.size(), spheres.size());
		for (size_t i = 0; i < count; i++) {
			spheres[i]->SetTranslate(points[i].position);
			spheres[i]->SetRotate(points[i].rotate);
			spheres[i]->Update();
			spheres[i]->Draw();
		}

	}

	// 停止中・再生中どちらでも、Empty の RailPointComponent を結ぶ線を表示する。
	DrawRailLine();

#endif 
}

Vector3 RailCamera::Evaluate(float distance) 
{
	if (points.size() < 4)
	{
		return Vector3{ 0,0,0 };
	} 
	float maxT = (float)(points.size() - 3);
	// distance制限 
	if (distance < 0.0f) distance = 0.0f;
	if (distance > maxT - 0.001f) distance = maxT - 0.001f;
	int i = (int)distance;
	float localT = distance - i;
	Vector3 p0 = points[i].position;
	Vector3 p1 = points[i + 1].position;
	Vector3 p2 = points[i + 2].position;
	Vector3 p3 = points[i + 3].position;
	return CatmullRom(p0, p1, p2, p3, localT); 
}
Vector3 RailCamera::EvaluateRotate(float distance) {
	if (points.size() < 4) {
		return Vector3{ 0, 0, 0 };
	}
	float maxT = (float)(points.size() - 3);

	// distance制限 
	if (distance < 0.0f) distance = 0.0f;
	if (distance > maxT - 0.001f) distance = maxT - 0.001f;

	int i = (int)distance;
	float localT = distance - i;

	Vector3 r0 = points[i].rotate;
	Vector3 r1 = points[i + 1].rotate;
	Vector3 r2 = points[i + 2].rotate;
	Vector3 r3 = points[i + 3].rotate;

	// 位置と同様に CatmullRom で補間
	return CatmullRom(r0, r1, r2, r3, localT);
}


Vector3 RailCamera::GetForward(float distance) 
{
	float look = 0.01f;
	Vector3 p0 = Evaluate(distance);
	Vector3 p1 = Evaluate(distance + look);
	return Normalize(p1 - p0);
} 

void RailCamera::AddPoint(Vector3 pos,Vector3 rotate)
{ 
	RailPoint p{}; 

	p.position = pos;
	p.rotate = rotate;
	points.push_back(p); 
	auto obj = std::make_unique<Object>();
	obj->Initialize(camera.get());
	obj->SetModel("cube.gltf");
	obj->SetScale(Vector3(0.5f, 0.5f, 0.5f)); 
	obj->SetTranslate(pos);
	obj->SetRotate(rotate);
	spheres.push_back(std::move(obj)); 
}

void RailCamera::DrawRailLine() {
	if (points.size() < 2) return;

	// 毎フレーム前回の線をクリアする
	railLine->Clear();

	// Catmull-Rom が使える 4 点に達するまで、Empty の配置順が分かる直線を描く。
	// これにより、制御点を作成した直後から接続状態を確認できる。
	if (points.size() < 4) {
		for (size_t index = 1; index < points.size(); ++index) {
			railLine->AddLine(points[index - 1].position, points[index].position);
		}
		railLine->Update();
		LineCommon::GetInstance()->SetCommonPipelineState();
		railLine->Draw();
		return;
	}

	const float tStart = 0.0f;
	const float tEnd = static_cast<float>(points.size() - 3);

	// 線の分割数（数値を大きくするほど滑らかになります）
	int segmentCount = 200;
	float step = tEnd / (float)segmentCount;

	Vector3 prevPos = Evaluate(tStart);

	// スプライン曲線を細かく区切って線分を追加していく
	for (float t = tStart + step; t <= tEnd + 0.001f; t += step) {
		if (t > tEnd) t = tEnd; // オーバーフロー防止

		Vector3 currentPos = Evaluate(t);
		railLine->AddLine(prevPos, currentPos); // 始点と終点を追加
		prevPos = currentPos;
	}

	// 溜め込んだ頂点データをバッファに更新
	railLine->Update();

	// ライン用の共通パイプラインを設定して描画
	LineCommon::GetInstance()->SetCommonPipelineState();
	railLine->Draw();
}

void RailCamera::UpdateGizmo() {
	if (selectedPoint < 0 || selectedPoint >= (int)points.size()) return;

	// ビュー・プロジェクション行列をカメラから取得
	Matrix4x4 view = camera->GetViewMatrix();
	Matrix4x4 proj = camera->GetProjectionMatrix();

	// ★ 位置(Translate) と 回転(Rotate) を組み合わせたワールド行列を作成
	Matrix4x4 matRotate = MakeRotateMatrix(points[selectedPoint].rotate); // ※プロジェクトの回転行列作成関数
	Matrix4x4 matTranslate = MakeTranslateMatrix(points[selectedPoint].position);
	Matrix4x4 world = matRotate * matTranslate;

#ifdef USE_IMGUI

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::BeginFrame();

	// ウィンドウ全体をギズモの描画範囲に
	ImGuizmo::SetRect(0, 0,
		(float)windowAPI_->kClientWidth,
		(float)windowAPI_->kClientHeight);

	// ★ UNIVERSAL（位置・回転を同時に扱うギズモ）に変更
	// 移動だけにする場合は ImGuizmo::TRANSLATE 、回転だけなら ImGuizmo::ROTATE
	ImGuizmo::Manipulate(
		&view.m[0][0],   // ビュー行列
		&proj.m[0][0],   // プロジェクション行列
		ImGuizmo::UNIVERSAL,  // ★ 位置＋回転の操作
		ImGuizmo::WORLD,      // ワールド空間
		&world.m[0][0]        // 操作対象のワールド行列
	);

	// ★ 操作後に位置と回転の両方を分解して反映
	if (ImGuizmo::IsUsing()) {
		float translation[3], rotation[3], scale[3];
		// ImGuizmo の行列分解関数（回転は「度数法(Degree)」で返ってきます）
		ImGuizmo::DecomposeMatrixToComponents(&world.m[0][0], translation, rotation, scale);

		// 位置の更新
		points[selectedPoint].position = { translation[0], translation[1], translation[2] };

		// 回転の更新（度数法からラジアンへ変換してセット）
		points[selectedPoint].rotate = {
			rotation[0] * (3.14159265f / 180.0f),
			rotation[1] * (3.14159265f / 180.0f),
			rotation[2] * (3.14159265f / 180.0f)
		};
	}

#endif
}

void RailCamera::SelectPointByMouse() {
#ifdef USE_IMGUI 
	// マウス座標をレイに変換してsphereとの当たり判定
	ImVec2 mousePos = ImGui::GetMousePos();
	float mx = mousePos.x / windowAPI_->kClientWidth * 2.0f - 1.0f;
	float my = -(mousePos.y / windowAPI_->kClientHeight * 2.0f - 1.0f);

	Matrix4x4 invVP = Inverse(camera->GetViewProjectionMatrix());

	Vector3 rayNear = VectorTransform({ mx, my, 0.0f }, invVP);
	Vector3 rayFar = VectorTransform({ mx, my, 1.0f }, invVP);
	Vector3 rayDir = Normalize(rayFar - rayNear);

	float minDist = FLT_MAX;
	int hit = -1;
	for (int i = 0; i < (int)points.size(); i++) {
		// 簡易球判定（半径0.5f）
		float dist = RaySphereIntersect(rayNear, rayDir, points[i].position, 0.5f);
		if (dist > 0 && dist < minDist) {
			minDist = dist;
			hit = i;
		}
	}
	selectedPoint = hit;
#endif 
}

Vector3 RailCamera::CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) { 
	float t2 = t * t; float t3 = t2 * t;
	return( (p1 * 2.0f) + (-p0 + p2) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3)* t2 + (-p0 + p1 * 3 - p2 * 3 + p3) * t3 ) * 0.5f;
}

void RailCamera::UpdateCameraDirection() {
	// 進行方向を向かせる
	Vector3 forward = GetForward(railT);
	// forwardからオイラー角を計算
	float yaw = atan2f(forward.x, forward.z);
	float pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
	cameraTransform.rotate = { pitch, yaw ,0.0f };
	// カメラ回転を更新
	camera->SetRotate(cameraTransform.rotate);
}

void RailCamera::UpdateCameraTarget() {
	// --- 変更箇所：指定した注視点に向かうベクトルを計算 ---
	Vector3 forward = Normalize(targetPosition - cameraTransform.translate);

	// forwardからオイラー角を計算
	float yaw = atan2f(forward.x, forward.z); 
		float pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
		cameraTransform.rotate = { pitch, yaw ,0.0f };
	// カメラ回転を更新
	camera->SetRotate(cameraTransform.rotate);
}

void RailCamera::UpdateCameraSpecified() {
	cameraTransform.rotate = EvaluateRotate(railT);
	camera->SetRotate(cameraTransform.rotate);
}

