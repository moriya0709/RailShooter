#include "SceneManager.h"

std::unique_ptr <SceneManager> SceneManager::instance = nullptr;

void SceneManager::Update(float deltaTime, float unscaledDeltaTime) {
	// シーン切り替え処理
	if (nextScene_) {
		// 旧シーンを終了してから所有権を移すため、リソースの解放順序が安定する。
		if (scene_) {
			scene_->Finalize();
		}

		// シーン切り換え
		scene_ = std::move(nextScene_);
		currentSceneName_ = std::move(nextSceneName_);
		// シーンをまたいで時止めが持ち越されないよう、切り替え時は通常速度へ戻す。
		timeScale_ = 1.0f;

		// 初期化より先に管理者を渡し、Initialize 内からのシーン操作を可能にする。
		scene_->SetSceneManager(this);

		// 次シーンを初期化
		scene_->Initialize();

	}

	// 切り替え直後の新シーンも同じフレームから更新する。
	scene_->Update(deltaTime, unscaledDeltaTime);

}

void SceneManager::SetTimeScale(float timeScale) {
	timeScale_ = (std::clamp)(timeScale, 0.0f, 1.0f);
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
	auto nextScene = sceneFactory_->CreateScene(sceneName);
	if (nextScene) {
		nextScene_ = std::move(nextScene);
		nextSceneName_ = sceneName;
		// フェード／遷移先が停止したままになることを防ぐ。
		timeScale_ = 1.0f;
	}

}

SceneManager::~SceneManager() {
	// 最後のシーンの終了と解放
	if (scene_) {
		scene_->Finalize();
	}
}
