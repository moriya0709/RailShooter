#pragma once
#include "Component.h"
#include "Model.h"
#include "CommonStructs.h"

class Model;
class Camera;
class DirectXCommon;

class ModelRendererComponent : public Component {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	// 通常描画の後に呼び出す、モデル輪郭専用の描画パス。
	void DrawOutline();

	void SetModel(const std::string& filePath);
	Model* GetModel() const { return model_; }
	const std::string& GetModelPath() const { return modelPath_; }
	// このレンダラーだけのベースカラーテクスチャ上書き。空文字列でモデル本来のテクスチャに戻す。
	void SetTextureOverride(const std::string& filePath);
	const std::string& GetTextureOverridePath() const { return textureOverridePath_; }
	// カメラ距離に応じて High / Medium / Low の 3 段階で描画モデルを切り替える。
	// mediumDistance 未満は High、lowDistance 未満は Medium、それ以上は Low。
	void SetLodModels(const std::string& highModelPath, const std::string& mediumModelPath,
		const std::string& lowModelPath);
	void SetLodDistances(float mediumDistance, float lowDistance);
	bool HasLod() const { return !lodHighModelPath_.empty(); }
	const std::string& GetLodHighModelPath() const { return lodHighModelPath_; }
	const std::string& GetLodMediumModelPath() const { return lodMediumModelPath_; }
	const std::string& GetLodLowModelPath() const { return lodLowModelPath_; }
	float GetLodMediumDistance() const { return lodMediumDistance_; }
	float GetLodLowDistance() const { return lodLowDistance_; }
	// A value of 0 disables distance culling. Objects beyond this distance issue no draw calls.
	void SetMaxDrawDistance(float distance) { maxDrawDistance_ = distance; }
	float GetMaxDrawDistance() const { return maxDrawDistance_; }
	void SetOutlineEnabled(bool enabled) { outlineEnabled_ = enabled; }
	bool IsOutlineEnabled() const { return outlineEnabled_; }
	void SetOutlineThickness(float thickness);
	float GetOutlineThickness() const { return outlineThickness_; }
	void SetOutlineColor(const Vector4& color);
	const Vector4& GetOutlineColor() const { return outlineColor_; }
	void SetColorOverrideEnabled(bool enabled);
	bool IsColorOverrideEnabled() const { return colorOverrideEnabled_; }
	void SetColorOverride(const Vector4& color);
	const Vector4& GetColorOverride() const { return colorOverride_; }
	void SetColorOverrideUnlit(bool unlit);
	bool IsColorOverrideUnlit() const { return colorOverrideUnlit_; }

private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> outlineResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> motionBlurResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> colorOverrideResource;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;
	Outline* outlineData = nullptr;
	ViewData* viewData = nullptr;
	MotionBlur* motionBlurData = nullptr;
	ModelColorOverride* colorOverrideData = nullptr;

	// Transform
	Transform cameraTransform;
	// モーションブラー
	Matrix4x4 currentWVP_;

	// モデル
	Model* model_ = nullptr;
	std::string modelPath_;
	// 入力途中などで未解決のパスを保持しても描画を壊さないよう、実際にバインドする
	// 有効なテクスチャは別に管理する。
	std::string textureOverridePath_;
	std::string activeTextureOverridePath_;
	Model* highLodModel_ = nullptr;
	Model* mediumLodModel_ = nullptr;
	Model* lowLodModel_ = nullptr;
	std::string lodHighModelPath_;
	std::string lodMediumModelPath_;
	std::string lodLowModelPath_;
	float lodMediumDistance_ = 300.0f;
	float lodLowDistance_ = 600.0f;
	void UpdateLodModel();
	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	float maxDrawDistance_ = 0.0f;
	bool outlineEnabled_ = false;
	float outlineThickness_ = 0.01f;
	Vector4 outlineColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };
	bool colorOverrideEnabled_ = false;
	Vector4 colorOverride_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool colorOverrideUnlit_ = false;
	bool IsCulledByDistance() const;
	void UpdateColorOverrideData();
};
