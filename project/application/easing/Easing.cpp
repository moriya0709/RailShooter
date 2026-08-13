#include "Easing.h"

void Easing::Initialize() {
	// イージングの枠数
	// kEasePNum = 0;

	// 一般的なイージング
	LoadEasing("Easing/normal.csv", 0);
	// ワープ
	LoadEasing("Easing/warp.csv", 1);
	// セレクトUI
	LoadEasing("Easing/select.csv", 2);
	// タイトル
	LoadEasing("Easing/title.csv", 3);
	// トランジション
	LoadEasing("Easing/loop.csv", 4);
}

void Easing::Update() {
	// 時間
	// time++;

#pragma region ImGui
	// ImGui
#ifdef USE_IMGUI
	// ベジェ曲線の制御点
	ImGui::SliderFloat("[0].x", &kControlPoints[0].x, -500.0f, 500.0f);
	ImGui::SliderFloat("[0].y", &kControlPoints[0].y, -500.0f, 500.0f);
	ImGui::SliderFloat("[1].x", &kControlPoints[1].x, -500.0f, 500.0f);
	ImGui::SliderFloat("[1].y", &kControlPoints[1].y, -500.0f, 500.0f);
	ImGui::SliderFloat("[2].x", &kControlPoints[2].x, -500.0f, 500.0f);
	ImGui::SliderFloat("[2].y", &kControlPoints[2].y, -500.0f, 500.0f);
	ImGui::SliderFloat("[3].x", &kControlPoints[3].x, -500.0f, 500.0f);
	ImGui::SliderFloat("[3].y", &kControlPoints[3].y, -500.0f, 500.0f);

	// ファイル名
	ImGui::InputText("fileName", fileName, sizeof(fileName));

	// 制御点の読み込み
	if (ImGui::Button("Load")) {
		LoadEasing(fileName, kEasePNum);

		for (int i = 0; i < kNumControlPoints; i++) {
			// 制御点の座標をイージング用コントロールポイントに代入
			kControlPoints[i].x = easeP[kEasePNum][i].x;
			kControlPoints[i].y = easeP[kEasePNum][i].y;

			kControlPoints[i].x *= 150.0f;
			kControlPoints[i].y *= 150.0f;

			kControlPoints[i].x += 50.0f;
			kControlPoints[i].y += 50.0f;
		}
	}

	// 制御点の保存
	if (ImGui::Button("Save")) {
		EntryEasing(fileName, kEasePNum);
	}

	// イージングの枠への割り当て
	if (ImGui::Button("easeP Save")) {
		for (int i = 0; i < ControlPointsNum; i++) {
			// 制御点の座標をイージング用コントロールポイントに代入
			easeP[kEasePNum][i].x = kControlPoints[i].x;
			easeP[kEasePNum][i].y = kControlPoints[i].y;

			// イージング用に変換
			easeP[kEasePNum][i].x -= kControlPoints[0].x;
			easeP[kEasePNum][i].y -= kControlPoints[0].y;

			easeP[kEasePNum][i].x /= 150.0f;
			easeP[kEasePNum][i].y /= 150.0f;
		}
	}

	// イージングの枠
	if (ImGui::SliderInt("easePNum", &kEasePNum, 0, 9)) {
		for (int i = 0; i < kNumControlPoints; i++) {
			// 制御点の座標をイージング用コントロールポイントに代入
			kControlPoints[i].x = easeP[kEasePNum][i].x;
			kControlPoints[i].y = easeP[kEasePNum][i].y;

			kControlPoints[i].x *= 150.0f;
			kControlPoints[i].y *= 150.0f;

			kControlPoints[i].x += 50.0f;
			kControlPoints[i].y += 50.0f;
		}
	}

	// 初期値にリセット
	if (ImGui::Button("Reset")) {
		for (int i = 0; i < kNumControlPoints; i++) {
			kControlPoints[i] = { 50.0f + i * 50.0f, 50.0f + i * 50.0f };
		}
	}

	// ベジェ曲線のウィンドウ
	ImGui::Begin("Easing");

	// 現在のウィンドウのDrawListを取得
	draw_list = ImGui::GetWindowDrawList();

	// ウィンドウの左上のスクリーン座標
	ImVec2 origin = ImGui::GetCursorScreenPos();

	// すべての制御点に原点を加算して、スクリーン座標に変換
	controlPosA = ImVec2(kControlPoints[0].x + origin.x, kControlPoints[0].y + origin.y);
	controlPosB = ImVec2(kControlPoints[1].x + origin.x, kControlPoints[1].y + origin.y);
	controlPosC = ImVec2(kControlPoints[2].x + origin.x, kControlPoints[2].y + origin.y);
	controlPosD = ImVec2(kControlPoints[3].x + origin.x, kControlPoints[3].y + origin.y);

	// 制御点の表示とドラッグ処理
	for (int i = 0; i < kNumControlPoints; ++i) {
		ImVec2 screenPos = ImVec2(kControlPoints[i].x + origin.x, kControlPoints[i].y + origin.y);

		// インタラクティブ用の見えないボタン
		ImGui::SetCursorScreenPos(ImVec2(screenPos.x - 10.0f, screenPos.y - 10.0f));
		ImGui::InvisibleButton(("pt" + std::to_string(i)).c_str(), ImVec2(10.0f * 2, 10.0f * 2));

		// ドラッグ中なら位置を更新
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 delta = ImGui::GetIO().MouseDelta;
			kControlPoints[i].x += delta.x;
			kControlPoints[i].y += delta.y;
		}
	}

	ImGui::End();

#endif

#pragma endregion
}

void Easing::Draw() {
#ifdef USE_IMGUI
	// 三次ベジェ曲線の描画
	draw_list->AddBezierCubic(
		controlPosA, controlPosB, controlPosC, controlPosD, // 制御点
		IM_COL32(255, 0, 0, 255),                           // 色
		2.0f                                                // 線の太さ
	);

	// 制御点の描画
	draw_list->AddCircleFilled(controlPosA, 4.0f, IM_COL32(255, 255, 255, 255));
	draw_list->AddCircleFilled(controlPosB, 4.0f, IM_COL32(0, 255, 0, 255));
	draw_list->AddCircleFilled(controlPosC, 4.0f, IM_COL32(0, 255, 0, 255));
	draw_list->AddCircleFilled(controlPosD, 4.0f, IM_COL32(255, 255, 255, 255));
#endif

	//* UI *//
}

void Easing::Move(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.moveTime += timeSpeed;

	if (ui.moveTime > 1.0f) {
		ui.moveTime = 1.0f;
	}

	ui.moveEasedT = BezierEasing(ui.moveTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.transform.translate = Lerp(ui.startPos, ui.endPos, ui.moveEasedT);
}

void Easing::MoveV2(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.moveTime += timeSpeed;

	if (ui.moveTime > 1.0f) {
		ui.moveTime = 1.0f;
	}

	ui.moveEasedT = BezierEasing(ui.moveTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.pos = Lerp(ui.startPosV2, ui.endPosV2, ui.moveEasedT);
}

void Easing::Size(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.sizeTime += timeSpeed;

	if (ui.sizeTime > 1.0f) {
		ui.sizeTime = 1.0f;
	}

	ui.sizeEasedT = BezierEasing(ui.sizeTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.transform.scale = Lerp(ui.startSize, ui.endSize, ui.sizeEasedT);
}

void Easing::SizeV2(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.sizeTime += timeSpeed;

	if (ui.sizeTime > 1.0f) {
		ui.sizeTime = 1.0f;
	}

	ui.sizeEasedT = BezierEasing(ui.sizeTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.size = Lerp(ui.startSizeV2, ui.endSizeV2, ui.sizeEasedT);
}

void Easing::Rotation(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.rotationTime += timeSpeed;

	if (ui.rotationTime > 1.0f) {
		ui.rotationTime = 1.0f;
	}

	ui.rotationEasedT = BezierEasing(ui.rotationTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.transform.rotate = Lerp(ui.startRotation, ui.endRotation, ui.rotationEasedT);
}

void Easing::Number(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.numberTime += timeSpeed;

	if (ui.numberTime > 1.0f) {
		ui.numberTime = 1.0f;
	}

	ui.numberEasedT = BezierEasing(ui.numberTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.num = Lerp(ui.startNumber, ui.endNumber, ui.numberEasedT);
}

void Easing::Color(EasingSet& ui, float timeSpeed, int num) {
	// イージング処理

	ui.colorTime += timeSpeed;

	if (ui.colorTime > 1.0f) {
		ui.colorTime = 1.0f;
	}

	ui.colorEasedT = BezierEasing(ui.colorTime, easeP[num][0], easeP[num][1], easeP[num][2], easeP[num][3]);

	ui.color = Lerp(ui.startColor, ui.endColor, ui.colorEasedT);
}

float Easing::Lerp(const float& p0, const float& p1, float t) {
	float result;
	result = (1.0f - t) * p0 + t * p1;
	return result;
}


Vector2 Easing::Lerp(const Vector2& p0, const Vector2& p1, float t) {
	Vector2 result;
	result.x = (1.0f - t) * p0.x + t * p1.x;
	result.y = (1.0f - t) * p0.y + t * p1.y;
	return result;
}

Vector3 Easing::Lerp(const Vector3& p0, const Vector3& p1, float t) {
	Vector3 result;
	result.x = (1.0f - t) * p0.x + t * p1.x;
	result.y = (1.0f - t) * p0.y + t * p1.y;
	result.z = (1.0f - t) * p0.z + t * p1.z;
	return result;
}

Vector4 Easing::Lerp(const Vector4& p0, const Vector4& p1, float t) {
	Vector4 result;
	result.x = (1.0f - t) * p0.x + t * p1.x;
	result.y = (1.0f - t) * p0.y + t * p1.y;
	result.z = (1.0f - t) * p0.z + t * p1.z;
	result.w = (1.0f - t) * p0.w + t * p1.w;
	return result;
}

// 3次ベジェ曲線
Vector2 Easing::Bezier(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t) {
	Vector2 a = Lerp(p0, p1, t);
	Vector2 b = Lerp(p1, p2, t);
	Vector2 c = Lerp(p2, p3, t);
	Vector2 d = Lerp(a, b, t);
	Vector2 e = Lerp(b, c, t);
	return Lerp(d, e, t);
}

float Easing::BezierEasing(float t, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3) {
	Vector2 pt = Bezier(p0, p1, p2, p3, t);
	return pt.y;
}

// ファイル読み込み
void Easing::LoadEasing(const std::string& filePath, int num) {
	// ファイル読み込み
	std::ifstream file(filePath);
	if (!file.is_open()) {
		// ファイルがない場合はとりあえず何もしない（またはデフォルト値を入れる）
		return;
	}

	std::string line;
	int index = 0;

	while (std::getline(file, line) && index < 4) {
		std::stringstream easingStream(line);
		std::string x, y;

		if (std::getline(easingStream, x, ',') && std::getline(easingStream, y, ',')) {
			easeP[num][index].x = std::stof(x);
			easeP[num][index].y = std::stof(y);
			index++;
		}
	}

	file.close();
}

// ファイル書き込み
bool Easing::EntryEasing(const std::string& filePath, int num) {
	std::ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}

	for (int i = 0; i < 4; ++i) {
		file << easeP[num][i].x << "," << easeP[num][i].y << "\n";
	}

	file.close();
	return true;
}
