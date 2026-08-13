#pragma once
#include <unordered_map>
#include <string>
#include <memory>

class Camera;

class CameraManager {
public:
	static CameraManager* GetInstance();

	// カメラ追加
	void AddCamera(const std::string& name, Camera* camera);

	// アクティブカメラ設定
	void SetActiveCamera(const std::string& name);
	// アクティブカメラ取得
	Camera* GetActiveCamera() const;
	// 削除
	void RemoveCamera(const std::string& name);

	// 更新
	void Update();

	CameraManager() = default;
	~CameraManager() = default;
	CameraManager(CameraManager&) = delete;
	CameraManager& operator=(CameraManager&) = delete;

private:
	std::unordered_map<std::string, Camera*> cameras_;
	Camera* activeCamera_ = nullptr;

	// シングルトン
	static std::unique_ptr <CameraManager> instance;

};

