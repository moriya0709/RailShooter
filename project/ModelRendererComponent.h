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

	void SetModel(const std::string& filePath);
	Model* GetModel() const { return model_; }
	const std::string& GetModelPath() const { return modelPath_; }
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

private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> outlineResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> motionBlurResource;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;
	Outline* outlineData = nullptr;
	ViewData* viewData = nullptr;
	MotionBlur* motionBlurData = nullptr;

	// Transform
	Transform cameraTransform;
	// モーションブラー
	Matrix4x4 currentWVP_;

	// モデル
	Model* model_ = nullptr;
	std::string modelPath_;
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
};
