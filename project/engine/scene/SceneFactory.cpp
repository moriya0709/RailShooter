#include "SceneFactory.h"

namespace {

using SceneCreator = std::unique_ptr<BaseScene>(*)();

struct SceneDefinition {
	const char* id;
	SceneCreator create;
};

const std::vector<SceneDefinition>& GetSceneDefinitions() {
	static const std::vector<SceneDefinition> kSceneDefinitions = {
		{ "TITLE", []() -> std::unique_ptr<BaseScene> { return std::make_unique<TitleScene>(); } },
		{ "SELECT", []() -> std::unique_ptr<BaseScene> { return std::make_unique<SelectScene>(); } },
		{ "GAMEPLAY", []() -> std::unique_ptr<BaseScene> { return std::make_unique<GamePlayScene>(); } },
		{ "RESULTS", []() -> std::unique_ptr<BaseScene> { return std::make_unique<ResultsScene>(); } },
	};
	return kSceneDefinitions;
}

} // namespace

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	for (const SceneDefinition& definition : GetSceneDefinitions()) {
		if (sceneName == definition.id) {
			return definition.create();
		}
	}

	return nullptr;
}

const std::vector<std::string>& SceneFactory::GetSceneNames() const {
	static const std::vector<std::string> kSceneNames = [] {
		std::vector<std::string> names;
		for (const SceneDefinition& definition : GetSceneDefinitions()) {
			names.emplace_back(definition.id);
		}
		return names;
	}();
	return kSceneNames;
}
