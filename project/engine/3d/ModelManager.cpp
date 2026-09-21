#include "ModelManager.h"
#include "ModelCommon.h"
#include "Model.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

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

	const auto loadStart = std::chrono::steady_clock::now();

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model>model = std::make_unique<Model>();
	model->Initialize(modelCommon, dxCommon_,srvManager_, directoryPath, filePath);

	// モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));

	// 事前登録されているアニメーションは、モデルの実体生成直後に読み込む。
	const auto animations = animationDefinitions.find(filePath);
	if (animations != animationDefinitions.end()) {
		Model* loadedModel = models.at(filePath).get();
		for (const AnimationDefinition& animation : animations->second) {
			loadedModel->LoadAnimation(animation.name, animation.directoryPath, animation.filePath);
		}
	}

	const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - loadStart).count();
	Logger::Log("[ModelManager] Loaded " + filePath + " in " +
		std::to_string(elapsedMilliseconds) + " ms\n");

}

void ModelManager::RegisterModelsInResourceDirectory(const std::string& resourceDirectory) {
	namespace fs = std::filesystem;

	const fs::path resourcePath(resourceDirectory);
	std::error_code error;
	if (!fs::exists(resourcePath, error) || error) {
		Logger::Log("[ModelManager] Resource directory was not found: " + resourceDirectory + "\n");
		return;
	}

	std::vector<fs::path> modelFiles;
	fs::recursive_directory_iterator iterator(
		resourcePath,
		fs::directory_options::skip_permission_denied,
		error);
	const fs::recursive_directory_iterator end;
	while (!error && iterator != end) {
		const fs::directory_entry& entry = *iterator;
		if (entry.is_regular_file(error) && !error) {
			std::string extension = entry.path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".fbx") {
				modelFiles.push_back(entry.path());
			}
		}

		iterator.increment(error);
		if (error) {
			Logger::Log("[ModelManager] Failed while scanning Resource: " + error.message() + "\n");
			return;
		}
	}

	// 登録順を固定し、同名モデルがあるときも結果が毎回変わらないようにする。
	std::sort(modelFiles.begin(), modelFiles.end(), [](const fs::path& left, const fs::path& right) {
		return left.generic_string() < right.generic_string();
	});

	for (const fs::path& modelPath : modelFiles) {
		const std::string fileName = modelPath.filename().string();
		if (registeredModelPaths.contains(fileName)) {
			Logger::Log("[ModelManager] Skipped duplicate model file name: " + fileName + "\n");
			continue;
		}

		registeredModelPaths.insert_or_assign(fileName, modelPath.generic_string());
	}

	Logger::Log("[ModelManager] Registered " + std::to_string(registeredModelPaths.size()) + " model file(s) from " + resourceDirectory + ".\n");
}

// モデルの検索
Model* ModelManager::FindModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}

	// 登録済みなら、初めて使われるこのタイミングでモデル実体を読み込む。
	const auto registered = registeredModelPaths.find(filePath);
	if (registered != registeredModelPaths.end()) {
		const std::filesystem::path modelPath(registered->second);
		LoadModel(modelPath.parent_path().generic_string(), modelPath.filename().string());
		return models.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}

void ModelManager::LoadAnimation(const std::string& modelFilePath, const std::string& animationName, const std::string& directoryPath, const std::string& animFilePath) {
	// アニメーション定義を登録し、モデルが遅延ロードされた時に適用できるようにする。
	auto& animations = animationDefinitions[modelFilePath];
	const auto existing = std::find_if(animations.begin(), animations.end(), [&animationName](const AnimationDefinition& animation) {
		return animation.name == animationName;
	});
	if (existing != animations.end()) {
		existing->directoryPath = directoryPath;
		existing->filePath = animFilePath;
	} else {
		animations.push_back({ animationName, directoryPath, animFilePath });
	}

	// すでに実体がロード済みの場合だけ、ただちに追加する。
	Model* model = models.contains(modelFilePath) ? models.at(modelFilePath).get() : nullptr;
	if (model) {
		model->LoadAnimation(animationName, directoryPath, animFilePath);
	}
}

std::vector<std::string> ModelManager::GetLoadedModelNames() const {
	std::vector<std::string> modelNames;
	modelNames.reserve(registeredModelPaths.size());
	for (const auto& pair : registeredModelPaths) {
		modelNames.push_back(pair.first); // mapのキー（filePath）を追加
	}
	return modelNames;
}
