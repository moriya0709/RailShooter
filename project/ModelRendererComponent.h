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
	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
};

