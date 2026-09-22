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

namespace {

std::string NormalizeModelKey(const std::string& modelPath) {
	namespace fs = std::filesystem;
	fs::path normalized(modelPath);
	if (normalized.is_absolute()) {
		return normalized.lexically_normal().generic_string();
	}
	const std::string key = normalized.lexically_normal().generic_string();
	constexpr std::string_view kResourcePrefix = "Resource/";
	return key.starts_with(kResourcePrefix) ? key.substr(kResourcePrefix.size()) : key;
}

}

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
	namespace fs = std::filesystem;
	const fs::path fullPath = fs::path(directoryPath) / fs::path(filePath).filename();
	LoadModelFromPath(NormalizeModelKey(filePath), fullPath.generic_string());
}

void ModelManager::LoadModelFromPath(const std::string& cacheKey, const std::string& modelPathString) {
	// 読み込み済みモデルを検索
	if (models.contains(cacheKey)) {
		// 読み込み済みなら早期return
		return;
	}

	const auto loadStart = std::chrono::steady_clock::now();

	// モデルの生成とファイル読み込み、初期化
	const std::filesystem::path modelPath(modelPathString);
	std::unique_ptr<Model>model = std::make_unique<Model>();
	model->Initialize(modelCommon, dxCommon_,srvManager_, modelPath.parent_path().generic_string(), modelPath.filename().string());

	// モデルをmapコンテナに格納する
	models.insert(std::make_pair(cacheKey, std::move(model)));

	// 事前登録されているアニメーションは、モデルの実体生成直後に読み込む。
	const auto animations = animationDefinitions.find(cacheKey);
	if (animations != animationDefinitions.end()) {
		Model* loadedModel = models.at(cacheKey).get();
		for (const AnimationDefinition& animation : animations->second) {
			loadedModel->LoadAnimation(animation.name, animation.directoryPath, animation.filePath);
		}
	}

	const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - loadStart).count();
	Logger::Log("[ModelManager] Loaded " + cacheKey + " in " +
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
		const std::string relativePath = fs::relative(modelPath, resourcePath, error).generic_string();
		if (error) {
			Logger::Log("[ModelManager] Failed to create relative model path: " + error.message() + "\n");
			return;
		}
		// 相対パスは常に登録する。これにより city/Higth と city/Low の同名モデルを区別できる。
		registeredModelPaths.insert_or_assign(relativePath, modelPath.generic_string());
		// 既存レベルとの互換性のため、ファイル名だけの参照は並び順で最初のモデルに解決する。
		registeredModelPaths.try_emplace(fileName, modelPath.generic_string());
	}

	Logger::Log("[ModelManager] Registered " + std::to_string(registeredModelPaths.size()) + " model file(s) from " + resourceDirectory + ".\n");
}

// モデルの検索
Model* ModelManager::FindModel(const std::string& filePath) {
	const std::string cacheKey = NormalizeModelKey(filePath);
	// 読み込み済みモデルを検索
	if (models.contains(cacheKey)) {
		// 読み込みモデルを戻り値としてreturn
		return models.at(cacheKey).get();
	}

	// 登録済みなら、初めて使われるこのタイミングでモデル実体を読み込む。
	const auto registered = registeredModelPaths.find(cacheKey);
	if (registered != registeredModelPaths.end()) {
		LoadModelFromPath(cacheKey, registered->second);
		return models.at(cacheKey).get();
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
