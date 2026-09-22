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
	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	float maxDrawDistance_ = 0.0f;
};
