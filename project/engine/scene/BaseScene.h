#pragma once

class DirectXCommon;
class Camera;
class SceneManager;

class BaseScene {
public:
	// 初期化
	virtual void Initialize() = 0;
	// 更新
	// Game が計測したゲーム時間を受け取る。unscaledDeltaTime は時止め中も進む実時間。
	virtual void Update(float deltaTime, float unscaledDeltaTime) = 0;
	// 描画
	virtual void Draw2D() = 0;
	// ポストエフェクトの最終合成後にバックバッファへ描く UI。
	// ここへ描いた要素はブルームや色調補正を含む一切のポストエフェクトを受けない。
	virtual void DrawPostEffectExcluded2D() {}
	virtual void Draw3D() = 0;
	virtual void DrawPostEffectMask() = 0;
	// 終了
	virtual void Finalize() = 0;

	// setter
	virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }


	virtual ~BaseScene() = default;

private:
	// シーンマネージャー
	SceneManager* sceneManager_ = nullptr;

};
