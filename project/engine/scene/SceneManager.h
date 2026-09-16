#pragma once
#include <cassert>
#include <memory>

#include "BaseScene.h"
#include "AbstractSceneFactory.h"

class SceneManager {
public:
	// 更新
	void Update();
	// 描画
	void Draw2D();
	void Draw3D();

	// フレーム途中で破棄しないよう、切り替え要求は次回 Update 冒頭まで保留する。
	void SetNextScene(std::unique_ptr <BaseScene> nextScene) { nextScene_ = move(nextScene); }
	// シングルトンインスタンスの取得
	static SceneManager* GetInstance();

	// シーン名から具象シーンを生成するファクトリーを設定する。
	void SetSceneFactory(std::unique_ptr <AbstractSceneFactory> sceneFactory) { sceneFactory_ = move(sceneFactory); }
	// シーンの変更
	void ChangeScene(const std::string& sceneName);

	SceneManager() = default;
	~SceneManager();
	SceneManager(SceneManager&) = delete;
	SceneManager& operator=(SceneManager&) = delete;

private:
	// シングルトンインスタンス
	static std::unique_ptr <SceneManager> instance;

	// 今のシーン（実行中シーン）
	std::unique_ptr <BaseScene> scene_ = nullptr;
	// 次のシーン
	std::unique_ptr <BaseScene> nextScene_ = nullptr;
	// シーンファクトリー
	std::unique_ptr <AbstractSceneFactory> sceneFactory_ = nullptr;

};
