#pragma once
#include "AbstractSceneFactory.h"
#include "GamePlayScene.h"
#include "TitleScene.h"

class SceneFactory : public AbstractSceneFactory {
public:
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

private:
	int currentStyle;
	int currentStage;
};
