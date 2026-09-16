#include "SceneFactory.h"

std::unique_ptr <BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	// 生成責務をここに集約し、SceneManager が具象クラスを知る必要をなくす。
	std::unique_ptr <BaseScene> newScene = nullptr;

	if (sceneName == "TITLE") {
		newScene = std::make_unique <TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		newScene = std::make_unique <GamePlayScene>();
	}

	return newScene;
}
