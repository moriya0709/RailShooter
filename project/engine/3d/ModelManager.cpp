#include "ModelManager.h"
#include "ModelCommon.h"
#include "Model.h"
#include "DirectXCommon.h"

std::unique_ptr <ModelManager> ModelManager::instance = nullptr;

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);
}

// シングルトンインスタンスの取得
ModelManager* ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ModelManager>();
	}
	return instance.get();
}

// モデルファイルの読み込み
void ModelManager::LoadModel(const std::string& directoryPath, const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model>model = std::make_unique<Model>();
	model->Initialize(modelCommon, dxCommon_,srvManager_, directoryPath, filePath);

	// モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));


}

// モデルの検索
Model* ModelManager::FindModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}

void ModelManager::LoadAnimation(const std::string& modelFilePath, const std::string& animationName, const std::string& directoryPath, const std::string& animFilePath) {
	// 読み込み済みのモデルを検索
	Model* model = FindModel(modelFilePath);

	if (model) {
		// モデルが見つかったら、そのモデルに対してアニメーションを読み込む
		model->LoadAnimation(animationName, directoryPath, animFilePath);
	}
}

std::vector<std::string> ModelManager::GetLoadedModelNames() const {
	std::vector<std::string> modelNames;
	modelNames.reserve(models.size());
	for (const auto& pair : models) {
		modelNames.push_back(pair.first); // mapのキー（filePath）を追加
	}
	return modelNames;
}
