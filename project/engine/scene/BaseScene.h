#pragma once

class DirectXCommon;
class Camera;
class SceneManager;

class BaseScene {
public:
	// 初期化
	virtual void Initialize() = 0;
	// 更新
	virtual void Update() = 0;
	// 描画
	virtual void Draw2D() = 0;
	virtual void Draw3D() = 0;
	// 終了
	virtual void Finalize() = 0;

	// setter
	virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }


	virtual ~BaseScene() = default;

private:
	// シーンマネージャー
	SceneManager* sceneManager_ = nullptr;

};