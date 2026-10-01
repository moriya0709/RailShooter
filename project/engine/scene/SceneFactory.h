#pragma once
#include "AbstractSceneFactory.h"
#include "GamePlayScene.h"
#include "TitleScene.h"
#include "SelectScene.h"

class SceneFactory : public AbstractSceneFactory {
public:
	// シーン識別子を対応する具象シーンへ変換する。未対応名では nullptr を返す。
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

private:
	int currentStyle;
	int currentStage;
};
