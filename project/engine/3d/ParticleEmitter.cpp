#include "Particleemitter.h"
#include "particleManager.h"
#include "ImGuiManager.h"
#include <iostream>

void ParticleEmitter::Initialize(std::string name, const Transform& transform, uint32_t count, float frequency) {
	// 初期値を設定
	emitter = std::make_unique<Emitter>();
	emitter->color = {1.0f,1.0f,1.0f,1.0f};
	emitter->uvScale = {0.0f,0.0f};	// UVスケール
	emitter->uvOffset = { 0.0f ,0.0f};	// UVオフセット
	emitter->translate = { 0.0f,0.0f,0.0f };
	emitter->scale = { 1.0f,1.0f,1.0f };
	emitter->rotate = { 0.0f,0.0f,0.0f };
	emitter->isRandPosition = { 0,0,0 };	// ランダムな座標にするかどうか
	emitter->isRandScale = { 0,0,0 };	// ランダムなスケールにするかどうか
	emitter->isRandRotate = { 0,0,0 };	// ランダムな回転にするかどうか
	emitter->isRandVelocity = { 0,0,0 };	// ランダムに動かすかどうか
	emitter->isScaleChange = { 0,0,0 };	// スケール変更するかどうか
	emitter->isColorChange = { 0,0,0,0 };	// 色変更するかどうか
	emitter->finalColor = { 0.0f,0.0f,0.0f,0.0f };
	emitter->lifeTime = 5.0f;
	emitter->currentTime = 0.0f;
	emitter->colorChangeSpeed = 0.0f;
	emitter->scaleAdd = 0.0f;			// スケール変更量
	emitter->emissive = 0.0f;			// エミッシブ
	emitter->uvScrollSpeed = { 0.0f,0.0f };	// UVスクロール速度
	emitter->useNoise = 0;		// 0:通常 1:ノイズテクスチャ 2:両方
	emitter->burnColor = { 0.0f,0.0f,0.0f };		// ふちの色
	emitter->count = 1; //!< 発生数
	emitter->frequency = 1.0f; //!< 発生頻度
	emitter->frequencyTime = 0.0f; //!< 頻度用時刻
	emitter->randPosition = { 0.0f,0.0f };
	emitter->randScale = { 0.0f,0.0f };
	emitter->randRotate = { 0.0f,0.0f };
	emitter->randVelocity = { 0.0f,0.0f };
	this->name = name;
	blendMode = kBlendModeNormal;
}

void ParticleEmitter::Update() {
	// 時間経過によって発生させる
	emitter->frequencyTime += kDeltaTime; // 時刻を進める
	if (emitter->frequency <= emitter->frequencyTime) { // 頻度より大きいなら発生
	ParticleManager::GetInstance()->Emit(
		name,
		emitter.get(),
		blendMode
	);
	emitter->frequencyTime -= emitter->frequency; // 余計に過ぎた時間も紙して頻度計算する
	}
}

void ParticleEmitter::Emit() {
	ParticleManager::GetInstance()->Emit(
		name,
		emitter.get(),
		blendMode
	);

}

// アクティブ設定
void ParticleEmitter::SetActive(const std::string& name) {
	this->name = name;
}

void ParticleEmitter::SaveParticle(const std::string& filePath) {
	std::ofstream file(filePath, std::ios::binary);
	assert(file.is_open());
	
	// パーティクルの座標
	file << emitter->translate.x << "," << emitter->translate.y << "," << emitter->translate.z << "\n";
	// パーティクルのスケール
	file << emitter->scale.x << "," << emitter->scale.y << "," << emitter->scale.z << "\n";
	// パーティクルの回転
	file << emitter->rotate.x << "," << emitter->rotate.y << "," << emitter->rotate.z << "\n";
	// パーティクルの発生数
	file << emitter->count << "\n";
	// パーティクルの発生頻度
	file << emitter->frequency << "\n";
	// パーティクルのランダム座標
	file << emitter->isRandPosition.x << "," << emitter->isRandPosition.y << "," << emitter->isRandPosition.z << "\n";
	// パーティクルのランダムスケール
	file << emitter->isRandScale.x << "," << emitter->isRandScale.y << "," << emitter->isRandScale.z << "\n";
	// パーティクルのランダム回転
	file << emitter->isRandRotate.x << "," << emitter->isRandRotate.y << "," << emitter->isRandRotate.z << "\n";
	// パーティクルのランダム速度
	file << emitter->isRandVelocity.x << "," << emitter->isRandVelocity.y << "," << emitter->isRandVelocity.z << "\n";
	// パーティクルの色
	file << emitter->color.x << "," << emitter->color.y << "," << emitter->color.z << "," << emitter->color.w << "\n";
	// パーティクルの最終色【追加】
	file << emitter->finalColor.x << "," << emitter->finalColor.y << "," << emitter->finalColor.z << "," << emitter->finalColor.w << "\n";
	// パーティクルの色変化速度
	file << emitter->colorChangeSpeed << "\n";
	// パーティクルの色変化
	file << emitter->isColorChange.x << "," << emitter->isColorChange.y << "," << emitter->isColorChange.z << "," << emitter->isColorChange.w << "\n";
	// パーティクルのサイズ変化
	file << emitter->isScaleChange.x << "," << emitter->isScaleChange.y << "," << emitter->isScaleChange.z << "\n";
	// パーティクルの発生範囲
	file << emitter->randPosition.x << "," << emitter->randPosition.y << "\n";
	// パーティクルのスケール範囲
	file << emitter->randScale.x << "," << emitter->randScale.y << "\n";
	// パーティクルの回転範囲
	file << emitter->randRotate.x << "," << emitter->randRotate.y << "\n";
	// パーティクルの速度範囲
	file << emitter->randVelocity.x << "," << emitter->randVelocity.y << "\n";
	// パーティクルの寿命範囲
	file << distTime.a() << "," << distTime.b() << "\n";
	// パーティクルのサイズ追加数
	file << emitter->scaleAdd << "\n";
	// エミッシブ
	file << emitter->emissive << "\n";
	// ブレンドモード
	file << (int)blendMode << "\n";
	// UVスケール
	file << emitter->uvScale.x << "," << uvScale.y << "\n";
	// UVスクロール速度
	file << emitter->uvScrollSpeed.x << "," << uvScrollSpeed.y << "\n";
	// UVオフセット
	file << uvOffset.x << "," << uvOffset.y << "\n";
	// ノイズ使用
	file << useNoise << "\n";
	// ふちの色
	file << burnColor.x << "," << burnColor.y << "," << burnColor.z << "\n";
	
	
	file.close();
}

void ParticleEmitter::LoadParticle(const std::string& filePath) {
	// ファイル読み込み
	std::ifstream file(filePath);
	assert(file.is_open());

	std::string line;

	// パーティクルの座標
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f", &emitter->translate.x, &emitter->translate.y, &emitter->translate.z);
	}

	// パーティクルのスケール
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f", &emitter->scale.x, &emitter->scale.y, &emitter->scale.z);
	}

	// パーティクルの回転
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f", &emitter->rotate.x, &emitter->rotate.y, &emitter->rotate.z);
	}

	// パーティクルの発生数
	if (std::getline(file, line)) {
		emitter->count = std::stoi(line);
	}

	// パーティクルの発生頻度
	if (std::getline(file, line)) {
		emitter->frequency = std::stof(line);
	}

	// ランダム座標（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->isRandPosition.x = (a != 0);
		emitter->isRandPosition.y = (b != 0);
		emitter->isRandPosition.z = (c != 0);
	}

	// ランダムスケール（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->isRandScale.x = (a != 0);
		emitter->isRandScale.y = (b != 0);
		emitter->isRandScale.z = (c != 0);
	}

	// ランダム回転（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->isRandRotate.x = (a != 0);
		emitter->isRandRotate.y = (b != 0);
		emitter->isRandRotate.z = (c != 0);
	}

	// ランダム速度（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->isRandVelocity.x = (a != 0);
		emitter->isRandVelocity.y = (b != 0);
		emitter->isRandVelocity.z = (c != 0);
	}

	// 色
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f,%f", &emitter->color.x, &emitter->color.y, &emitter->color.z, &emitter->color.w);
	}

	// 最終色
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f,%f", &emitter->finalColor.x, &emitter->finalColor.y, &emitter->finalColor.z, &emitter->finalColor.w);
	}

	// 色変化速度【追加】
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f", &emitter->colorChangeSpeed);
	}

	// 色変化（bool）
	if (std::getline(file, line)) {
		int a, b, c, d;
		sscanf_s(line.c_str(), "%d,%d,%d,%d", &a, &b, &c, &d);
		emitter->isColorChange.x = (a != 0);
		emitter->isColorChange.y = (b != 0);
		emitter->isColorChange.z = (c != 0);
		emitter->isColorChange.w = (d != 0);
	}

	// サイズ変化（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->isScaleChange.x = (a != 0);
		emitter->isScaleChange.y = (b != 0);
		emitter->isScaleChange.z = (c != 0);
	}

	// 発生範囲
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->randPosition.x = (a != 0);
		emitter->randPosition.y = (b != 0);
	}

	// スケール範囲
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->randScale.x = (a != 0);
		emitter->randScale.y = (b != 0);
	}

	// 回転範囲
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->randRotate.x = (a != 0);
		emitter->randRotate.y = (b != 0);
	}

	// 速度範囲
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->randVelocity.x = (a != 0);
		emitter->randVelocity.y = (b != 0);
	}

	// 寿命範囲
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		emitter->randLifeTime.x = (a != 0);
		emitter->randLifeTime.y = (b != 0);
	}

	// サイズ追加数
	if (std::getline(file, line)) {
		emitter->scaleAdd = std::stof(line);
	}

	// エミッシブの読み込み
	if (std::getline(file, line)) {
		emitter->emissive = std::stof(line);
	}

	// ブレンドモードの読み込み
	if (std::getline(file, line)) {
		blendMode = (BlendMode)std::stoi(line);
	}

	// UVスケールの読み込み
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f", &emitter->uvScale.x, &emitter->uvScale.y);
	}

	// UVスクロール速度の読み込み
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f", &emitter->uvScrollSpeed.x, &emitter->uvScrollSpeed.y);
	}

	// UVオフセットの読み込み
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f", &emitter->uvOffset.x, &emitter->uvOffset.y);
	}

	// ノイズ使用の読み込み
	if (std::getline(file, line)) {
		int a;
		sscanf_s(line.c_str(), "%d", &a);
		emitter->useNoise = a;
	}

	// ふちの色
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f", &emitter->burnColor.x, &emitter->burnColor.y, &emitter->burnColor.z);
	}

	file.close();
}

void ParticleEmitter::Editor() {
#ifdef USE_IMGUI
	ImGui::Begin("Partocle");
	// パーティクルの座標変更
	ImGui::DragFloat3("translate", &emitter->translate.x, 0.01f, -100.0f, 100.0f);
	// パーティクルのスケール変更
	ImGui::DragFloat3("scale", &emitter->scale.x, 0.01f, -100.0f, 100.0f);
	// パーティクルの回転変更
	ImGui::DragFloat3("rotate", &emitter->rotate.x, 0.1f, 0.0f, 3.14f);

	// パーティクルのメッシュ
	if (ImGui::Button("obj", ImVec2(50, 50))) {
		SetActive("obj");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("ring", ImVec2(50, 50))) {
		SetActive("ring");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("cylinder", ImVec2(50, 50))) {
		SetActive("cylinder");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("cone", ImVec2(50, 50))) {
		SetActive("cone");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("sphere", ImVec2(50, 50))) {
		SetActive("sphere");
	}

	// パーティクルの発生数
	ImGui::SliderInt("EmitterCount", (int*)&emitter->count, 1, 100);
	// パーティクルの発生頻度
	ImGui::SliderFloat("EmitterFrequency", &emitter->frequency, 0.01f, 5.0f);
	// パーティクルのランダム座標
	bool boolRandPosX = (emitter->isRandPosition.x != 0);
	if (ImGui::Checkbox("RandPosition.x", &boolRandPosX)) {
		if (boolRandPosX) {
			emitter->isRandPosition.x = 1;
		} else {
			emitter->isRandPosition.x = 0;
		}
	}
	bool boolRandPosY = (emitter->isRandPosition.y != 0);
	if (ImGui::Checkbox("RandPosition.y", &boolRandPosY)) {
		if (boolRandPosY) {
			emitter->isRandPosition.y = 1;
		} else {
			emitter->isRandPosition.y = 0;
		}
	}
	bool boolRandPosZ = (emitter->isRandPosition.z != 0);
	if (ImGui::Checkbox("RandPosition.z", &boolRandPosZ)) {
		if (boolRandPosZ) {
			emitter->isRandPosition.z = 1;
		} else {
			emitter->isRandPosition.z = 0;
		}
	}
	// パーティクルのランダムスケール
	bool boolRandScaleX = (emitter->isRandScale.x != 0);
	if (ImGui::Checkbox("RandScale.x", &boolRandScaleX)) {
		if (boolRandScaleX) {
			emitter->isRandScale.x = 1;
		} else {
			emitter->isRandScale.x = 0;
		}
	}
	bool boolRandScaleY = (emitter->isRandScale.y != 0);
	if (ImGui::Checkbox("RandScale.y", &boolRandScaleY)) {
		if (boolRandScaleY) {
			emitter->isRandScale.y = 1;
		} else {
			emitter->isRandScale.y = 0;
		}
	}
	bool boolRandScaleZ = (emitter->isRandScale.z != 0);
	if (ImGui::Checkbox("RandScale.z", &boolRandScaleZ)) {
		if (boolRandScaleZ) {
			emitter->isRandScale.z = 1;
		} else {
			emitter->isRandScale.z = 0;
		}
	}
	// パーティクルのランダム回転
	bool boolRandRotateX = (emitter->isRandRotate.x != 0);
	if (ImGui::Checkbox("RandRotate.x", &boolRandRotateX)) {
		if (boolRandRotateX) {
			emitter->isRandRotate.x = 1;
		} else {
			emitter->isRandRotate.x = 0;
		}
	}
	bool boolRandRotateY = (emitter->isRandRotate.y != 0);
	if (ImGui::Checkbox("RandRotate.y", &boolRandRotateY)) {
		if (boolRandRotateY) {
			emitter->isRandRotate.y = 1;
		} else {
			emitter->isRandRotate.y = 0;
		}
	}
	bool boolRandRotateZ = (emitter->isRandRotate.z != 0);
	if (ImGui::Checkbox("RandRotate.z", &boolRandRotateZ)) {
		if (boolRandRotateZ) {
			emitter->isRandRotate.z = 1;
		} else {
			emitter->isRandRotate.z = 0;
		}
	}
	// パーティクルのランダム速度
	bool boolRandVelocityX = (emitter->isRandVelocity.x != 0);
	if (ImGui::Checkbox("RandVelocity.x", &boolRandVelocityX)) {
		if (boolRandVelocityX) {
			emitter->isRandVelocity.x = 1;
		} else {
			emitter->isRandVelocity.x = 0;
		}
	}
	bool boolRandVelocityY = (emitter->isRandVelocity.y != 0);
	if (ImGui::Checkbox("RandVelocity.y", &boolRandVelocityY)) {
		if (boolRandVelocityY) {
			emitter->isRandVelocity.y= 1;
		} else {
			emitter->isRandVelocity.y = 0;
		}
	}
	bool boolRandVelocityZ = (emitter->isRandVelocity.z != 0);
	if (ImGui::Checkbox("RandVelocity.z", &boolRandVelocityZ)) {
		if (boolRandVelocityZ) {
			emitter->isRandVelocity.z = 1;
		} else {
			emitter->isRandVelocity.z = 0;
		}
	}
	// パーティクルの色
	ImGui::ColorEdit4("ParticleColor", &emitter->color.x);
	ImGui::ColorEdit4("ParticleFinalColor", &emitter->finalColor.x);
	ImGui::SliderFloat("ColorChangeSpeed", &emitter->colorChangeSpeed, 1.0f, 20.0f);
	// パーティクルの色変化
	bool boolColorChangeX = (emitter->isColorChange.x != 0);
	if (ImGui::Checkbox("ColorChange.x", &boolColorChangeX)) {
		if (boolColorChangeX) {
			emitter->isColorChange.x = 1;
		} else {
			emitter->isColorChange.x = 0;
		}
	}
	bool boolColorChangeY = (emitter->isColorChange.y != 0);
	if (ImGui::Checkbox("ColorChange.y", &boolColorChangeY)) {
		if (boolColorChangeY) {
			emitter->isColorChange.y = 1;
		} else {
			emitter->isColorChange.y = 0;
		}
	}
	bool boolColorChangeZ = (emitter->isColorChange.z != 0);
	if (ImGui::Checkbox("ColorChange.z", &boolColorChangeZ)) {
		if (boolColorChangeZ) {
			emitter->isColorChange.z = 1;
		} else {
			emitter->isColorChange.z = 0;
		}
	}
	bool boolColorChangeW = (emitter->isColorChange.w != 0);
	if (ImGui::Checkbox("ColorChange.w", &boolColorChangeW)) {
		if (boolColorChangeW) {
			emitter->isColorChange.w = 1;
		} else {
			emitter->isColorChange.w = 0;
		}
	}
	// パーティクルのサイズ変化
	bool boolScaleChangeX = (emitter->isScaleChange.x != 0);
	if (ImGui::Checkbox("ScaleChange.x", &boolScaleChangeX)) {
		if (boolScaleChangeX) {
			emitter->isScaleChange.x = 1;
		} else {
			emitter->isScaleChange.x = 0;
		}
	}
	bool boolScaleChangeY = (emitter->isScaleChange.y != 0);
	if (ImGui::Checkbox("ScaleChange.y", &boolScaleChangeY)) {
		if (boolScaleChangeY) {
			emitter->isScaleChange.y = 1;
		} else {
			emitter->isScaleChange.y = 0;
		}
	}
	bool boolScaleChangeZ = (emitter->isScaleChange.z != 0);
	if (ImGui::Checkbox("ScaleChange.z", &boolScaleChangeZ)) {
		if (boolScaleChangeZ) {
			emitter->isScaleChange.z = 1;
		} else {
			emitter->isScaleChange.z = 0;
		}
	}
	// エミッシブ
	ImGui::DragFloat("Emissive", &emitter->emissive, 0.1f, 0.0f, 100.0f);

	// ★ImGuiでブレンドモードを選択可能にする
	const char* items[] = { "Normal", "Add", "Subtract", "Multiply", "Screen" };
	int currentItem = (int)blendMode;
	if (ImGui::Combo("BlendMode", &currentItem, items, IM_ARRAYSIZE(items))) {
		blendMode = (BlendMode)currentItem;
	}

	// パーティクルの発生範囲
	ImGui::SliderFloat2("randPosition", &emitter->randPosition.x, -100.0f, 100.0f);
	// パーティクルのスケール範囲
	ImGui::SliderFloat2("randScale", &emitter->randScale.x, -100.0f, 100.0f);
	// パーティクルの回転範囲
	ImGui::SliderFloat2("randRotate", &emitter->randRotate.x, -360.0f, 360.0f);
	// パーティクルの速度範囲
	ImGui::SliderFloat2("randVelocity", &emitter->randVelocity.x, -100.0f, 100.0f);
	// 寿命範囲
	ImGui::SliderFloat2("randLifeTime", &emitter->randLifeTime.x, 0.0f, 10.0f);
	// パーティクルのサイズ追加数
	ImGui::SliderFloat("scaleAdd", &emitter->scaleAdd, -0.1f, 0.1f);
	// uvスケール
	ImGui::SliderFloat2("uvScale", (float*)&emitter->uvScale, 0.0f, 10.0f);
	ImGui::SliderFloat2("uvScrollSpeed", (float*)&emitter->uvScrollSpeed, 0.0f, 10.0f);
	ImGui::SliderFloat2("uvOffset", (float*)&emitter->uvOffset, 0.0f, 10.0f);
	ImGui::SliderInt("useNoise", &emitter->useNoise, 0, 2);
	// 燃えるエフェクトの色
	ImGui::ColorEdit3("burnColor", &emitter->burnColor.x);

	// ファイル名
	ImGui::InputText("FileName", fileName, IM_ARRAYSIZE(fileName));

	// セーブ
	if (ImGui::Button("SaveParticles")) {
		std::string path = "Resource/Particle/";
		path += fileName;
		path += ".csv";   // 拡張子を自動付与

		SaveParticle(path);
	}
	// ロード
	if (ImGui::Button("LoadParticles")) {
		std::string path = "Resource/Particle/";
		path += fileName;
		path += ".csv";   // 拡張子を自動付与

		LoadParticle(path);
	}
	ImGui::End();

#endif
}
