#include "SceneManager.h"

std::unique_ptr <SceneManager> SceneManager::instance = nullptr;

void SceneManager::Update() {
	// シーン切り替え処理
	if (nextScene_) {
		// 旧シーンを終了してから所有権を移すため、リソースの解放順序が安定する。
		if (scene_) {
			scene_->Finalize();
		}

		// シーン切り換え
		scene_ = std::move(nextScene_);

		// 初期化より先に管理者を渡し、Initialize 内からのシーン操作を可能にする。
		scene_->SetSceneManager(this);

		// 次シーンを初期化
		scene_->Initialize();

	}

	// 切り替え直後の新シーンも同じフレームから更新する。
	scene_->Update();

}

void SceneManager::Draw2D() {
	scene_->Draw2D();
}
void SceneManager::Draw3D() {
	scene_->Draw3D();
}

SceneManager* SceneManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <SceneManager>();
	}
	return instance.get();
}

void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	// 次シーンを生成
	nextScene_ = sceneFactory_->CreateScene(sceneName);

}

SceneManager::~SceneManager() {
	// 最後のシーンの終了と解放
	if (scene_) {
		scene_->Finalize();
	}
}
