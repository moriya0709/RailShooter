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
	// モデルの検索
	Model* FindModel(const std::string& filePath);
	
	// 追加のアニメーションを読み込み
	void LoadAnimation(const std::string& modelFilePath, const std::string& animationName, const std::string& directoryPath, const std::string& animFilePath);

	// ロード済みモデルのファイル名一覧を取得
	std::vector<std::string> GetLoadedModelNames() const;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;

private:
	static std::unique_ptr <ModelManager> instance;
	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;


	// モデル共通部
	ModelCommon* modelCommon = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;

};

