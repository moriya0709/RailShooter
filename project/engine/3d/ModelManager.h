#pragma once
#include <map>
#include <string>
#include <memory>
#include <vector>

class Model;
class ModelCommon;
class DirectXCommon;
class SrvManager;

class ModelManager {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	// シングルトンインスタンスの取得
	static ModelManager* GetInstance();

	// モデルファイルの読み込み
	void LoadModel(const std::string& directoryPath, const std::string& filePath);
	// Resource 以下にある対応モデルの場所だけを再帰的に登録する
	// モデル本体は FindModel が呼ばれた時点で読み込まれる
	// 対応形式: .obj, .gltf, .glb, .fbx
	void RegisterModelsInResourceDirectory(const std::string& resourceDirectory = "Resource");
	// モデルの検索（未ロードなら登録済みのファイルから遅延ロードする）
	Model* FindModel(const std::string& filePath);
	
	// 追加のアニメーションを読み込み
	void LoadAnimation(const std::string& modelFilePath, const std::string& animationName, const std::string& directoryPath, const std::string& animFilePath);

	// 登録済みモデルのファイル名一覧を取得
	std::vector<std::string> GetLoadedModelNames() const;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;

private:
	struct AnimationDefinition {
		std::string name;
		std::string directoryPath;
		std::string filePath;
	};

	static std::unique_ptr <ModelManager> instance;
	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;
	// モデルファイル名と実ファイルパスの対応表（実体は未ロード）
	std::map<std::string, std::string> registeredModelPaths;
	// モデルが遅延ロードされた時に適用するアニメーション
	std::map<std::string, std::vector<AnimationDefinition>> animationDefinitions;


	// モデル共通部
	ModelCommon* modelCommon = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;

};
